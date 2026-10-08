#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "sprites.h"
#include "comida.h"
#include "iconos.h"
#include "fondo.h"
#include "cocina.h"
#include "fondo_menu.h"
#include "fondo_comida.h"
#include "efectos.h"
#include "fondo_info.h"

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* urlBase = "https://tamagotchi-api-9vk6.onrender.com";

// Pines de la pantalla (SPI de hardware)
#define TFT_CS 10
#define TFT_DC 13
#define TFT_RST 14
#define TFT_MOSI 11
#define TFT_SCK 12
#define TFT_MISO 16
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#define PIN_NAV 4
#define PIN_SELECT 5
#define PIN_BACK 6
#define PIN_BUZZER 7
#define SONIDO_ACTIVADO true   // false = Roberto en silencio

// Posiciones de Roberto en cada pantalla (ajustá si no quedan bien)
#define ROBERTO_X 60
#define ROBERTO_Y 170
#define ROBERTO_DORMIR_X 48
#define ROBERTO_DORMIR_Y 130
// Cocina: Roberto mira a la izquierda, asi que va sentado en la silla derecha
#define ROBERTO_COCINA_X 138
#define ROBERTO_COCINA_Y 140
#define ROBERTO_COCINA_ESCALA 4
// La comida apoyada sobre la mesa, del lado de Roberto
#define COMIDA_MESA_X 118
#define COMIDA_MESA_Y 152
#define COMIDA_ESCALA 2
// Zona de la cocina que se redibuja en cada cuadro (cubre a Roberto + la comida)
#define COCINA_ZONA_X 100
#define COCINA_ZONA_Y 140
#define COCINA_ZONA_W 110
#define COCINA_ZONA_H 96

#define DURACION_CINEMATICA 3000
#define MORDIDAS 3

// ---------- Stats ----------
// volatile: los actualiza la tarea de red (otro nucleo) y los lee el loop
volatile int hambreActual = 100;
volatile int suenoActual = 100;
volatile int dopaminaActual = 100;
volatile bool statsNuevos = false;
volatile bool durmiendoServidor = false;      // el servidor dice que Roberto esta dormido
volatile bool primeraConsulta = true;         // para retomar el sueno si se reinicio la placa
volatile uint32_t intervaloConsultaMs = 60000;  // cada cuanto se piden los stats (mas seguido al dormir)

// Cola de acciones para mandar al servidor sin trabar la pantalla
QueueHandle_t colaAcciones;

// ---------- Pantallas ----------
enum Pantalla { P_PRINCIPAL, P_MENU, P_COMIDA, P_COCINA, P_DORMIR, P_INFO, P_INFO_RESPUESTA, P_JUGAR };
Pantalla pantallaActual = P_PRINCIPAL;

int cursorMenu = 0;
const char* menuItems[4] = {"Comida", "Dormir", "Jugar", "Info"};
// Centros de los circulos de fondo_menu.h (medidos sobre el dibujo)
const int menuX[4] = {56, 58, 182, 182};
const int menuY[4] = {49, 144, 143, 49};

int cursorComida = 0;
int comidaElegida = 0;  // copia de cursorComida al entrar a la cocina
const char* comidaItems[2] = {"Leche", "Torta"};

int cursorInfo = 0;
// Las preguntas se escriben en preguntas.json (en el servidor) y Roberto las baja al prender.
// Estas son solo por si no hay internet. El fondo tiene lugar para 6.
#define MAX_PREGUNTAS 6
#define N_PREGUNTAS_FIJAS 4
const char* preguntasFijas[N_PREGUNTAS_FIJAS] = {"Nombre", "Edad", "Comida favorita", "Juego favorito"};
const char* clavesFijas[N_PREGUNTAS_FIJAS] = {"nombre", "edad", "comida_favorita", "juego_favorito"};
char preguntasBajadas[MAX_PREGUNTAS][40];
char clavesBajadas[MAX_PREGUNTAS][40];
int nPreguntasBajadas = 0;
volatile bool preguntasListas = false;   // true cuando ya se bajaron del servidor
bool infoDibujadaConBajadas = false;

String respuestaActual = "";

unsigned long cocinaInicio = 0;
int frameCocinaDibujado = -1;
int mordidasDibujadas = -1;

// Principal: Roberto animado. Una animacion "especial" (saludo, festejo) dura un rato
// y despues vuelve al reposo, que depende de los stats.
enum AnimPrincipal { ANIM_REPOSO, ANIM_SALUDO, ANIM_FESTEJO };
AnimPrincipal animPrincipal = ANIM_REPOSO;
unsigned long animInicio = 0;
unsigned long animDuracion = 0;
const char* const* spriteDibujado = nullptr;  // para repintar solo si cambia algo
int faseEstrellaDibujada = -1;

// Info: Roberto "habla" mientras dura la respuesta
unsigned long hablarInicio = 0;
unsigned long hablarDuracion = 0;
int frameHablarDibujado = -1;

int frameDormir = 0;
unsigned long ultimoCambioDormir = 0;

bool navAnterior = false, selectAnterior = false, backAnterior = false;

// ---------- Sonido (buzzer) ----------
// Melodias cortitas y suaves. Cada nota: {frecuencia en Hz, duracion en ms}.
// Frecuencia 0 = silencio. La melodia termina con {0, 0}.
struct Nota { uint16_t freq; uint16_t ms; };
#define DO5 523
#define MI5 659
#define SOL5 784
#define LA5 880
#define DO6 1047

const Nota SONIDO_BOTON[]    = {{1800, 12}, {0, 0}};
const Nota SONIDO_SALUDO[]   = {{SOL5, 90}, {0, 40}, {DO6, 140}, {0, 0}};
const Nota SONIDO_FESTEJO[]  = {{DO5, 90}, {MI5, 90}, {SOL5, 90}, {DO6, 220}, {0, 0}};
const Nota SONIDO_MORDIDA[]  = {{330, 35}, {0, 0}};
const Nota SONIDO_DORMIR[]   = {{SOL5, 160}, {MI5, 160}, {DO5, 300}, {0, 0}};
const Nota SONIDO_HABLAR[]   = {{LA5, 30}, {0, 0}};
const Nota SONIDO_NO[]       = {{MI5, 80}, {0, 40}, {DO5, 120}, {0, 0}};

// tone() del ESP32 tiene su propia cola: las notas se mandan todas juntas y suenan
// una detras de otra solas, sin trabar la pantalla ni los botones.
void sonar(const Nota* melodia) {
    if (!SONIDO_ACTIVADO) return;
    for (int i = 0; melodia[i].ms > 0; i++) tone(PIN_BUZZER, melodia[i].freq, melodia[i].ms);
}

bool sePresiono(int pin, bool &anterior) {
    bool actual = (digitalRead(pin) == LOW);
    bool disparo = (actual && !anterior);
    anterior = actual;
    return disparo;
}

// ---------- Color ----------
uint16_t colorDeLetraPaleta(char c, const char* letras, const uint16_t* colores, int n) {
    for (int i = 0; i < n; i++) if (letras[i] == c) return colores[i];
    return ILI9341_BLACK;
}

// Oscurece un color RGB565 multiplicando cada canal por un factor (0.5 = mitad de brillo)
uint16_t oscurecer(uint16_t color565, float factor) {
    uint8_t r = (color565 >> 11) & 0x1F;
    uint8_t g = (color565 >> 5) & 0x3F;
    uint8_t b = color565 & 0x1F;
    r = (uint8_t)(r * factor);
    g = (uint8_t)(g * factor);
    b = (uint8_t)(b * factor);
    return (r << 11) | (g << 5) | b;
}

// ---------- Dibujo de sprites, con transparencia real ----------
// "g" puede ser la pantalla (tft) o un lienzo en memoria (GFXcanvas16).
// colMax: solo dibuja las columnas 0..colMax-1 (sirve para "morder" la comida).
void dibujarSpriteEn(Adafruit_GFX &g, int x, int y, const char* const* sprite,
                     const char* letras, const uint16_t* colores, int n, int escala,
                     float factor = 1.0, int colMax = 24) {
    for (int fy = 0; fy < 24; fy++) {
        for (int fx = 0; fx < colMax; fx++) {
            char c = sprite[fy][fx];
            if (c == '.') continue;
            uint16_t color = colorDeLetraPaleta(c, letras, colores, n);
            if (factor < 0.999) color = oscurecer(color, factor);
            g.fillRect(x + fx * escala, y + fy * escala, escala, escala, color);
        }
    }
}

void dibujarSpriteGenerico(int x, int y, const char* const* sprite,
                           const char* letras, const uint16_t* colores, int n, int escala, float factor = 1.0) {
    dibujarSpriteEn(tft, x, y, sprite, letras, colores, n, escala, factor);
}

void dibujarSprite(int x, int y, const char* const* sprite, int escala = 5, float factor = 1.0) {
    dibujarSpriteGenerico(x, y, sprite, PALETA_LETRAS, PALETA_COLORES, PALETA_N, escala, factor);
}

void dibujarIcono(int x, int y, const char* const* icono, uint16_t colorTinta) {
    for (int fy = 0; fy < 24; fy++) {
        for (int fx = 0; fx < 24; fx++) {
            if (icono[fy][fx] == 'X') tft.drawPixel(x + fx, y + fy, colorTinta);
        }
    }
}

// ---------- Fondos (RLE) ----------
void dibujarFondoRLE(const uint16_t* colores, const uint16_t* conteos, int runs, int ancho, int alto, float factor) {
    tft.startWrite();
    tft.setAddrWindow(0, 0, ancho, alto);
    for (int i = 0; i < runs; i++) {
        uint16_t color = pgm_read_word(&colores[i]);
        if (factor < 0.999) color = oscurecer(color, factor);
        uint16_t cuenta = pgm_read_word(&conteos[i]);
        tft.writeColor(color, cuenta);
    }
    tft.endWrite();
}

void dibujarFondo(float factor = 1.0) {
    dibujarFondoRLE(FONDO_COLOR, FONDO_CONTEO, FONDO_RUNS, FONDO_ANCHO, FONDO_ALTO, factor);
}

void dibujarFondoCocina(float factor = 1.0) {
    dibujarFondoRLE(FONDO_COCINA_COLOR, FONDO_COCINA_CONTEO, FONDO_COCINA_RUNS, FONDO_COCINA_ANCHO, FONDO_COCINA_ALTO, factor);
}

// Descomprime SOLO un rectangulo del fondo RLE dentro de un buffer en memoria.
// Sirve para animar: en vez de repintar toda la pantalla (lento, parpadea y
// mientras tanto no se leen los botones) se repinta solo la zona del sprite.
void fondoRegionEnBuffer(const uint16_t* colores, const uint16_t* conteos, int runs, int ancho,
                         int rx, int ry, int rw, int rh, float factor, uint16_t* buf) {
    uint32_t pos = 0;
    uint32_t inicio = (uint32_t)ry * ancho;
    uint32_t fin = (uint32_t)(ry + rh) * ancho;
    for (int i = 0; i < runs && pos < fin; i++) {
        uint32_t a = pos;
        uint32_t b = pos + pgm_read_word(&conteos[i]);
        pos = b;
        if (b <= inicio) continue;
        uint16_t color = pgm_read_word(&colores[i]);
        if (factor < 0.999) color = oscurecer(color, factor);
        if (a < inicio) a = inicio;
        if (b > fin) b = fin;
        // un run puede ocupar varias filas: se recorre de a pedazos de fila
        while (a < b) {
            int fila = a / ancho;
            int col = a % ancho;
            uint32_t finTramo = min(b, (uint32_t)(fila + 1) * ancho);
            int c0 = max(col, rx);
            int c1 = min(col + (int)(finTramo - a), rx + rw);
            for (int c = c0; c < c1; c++) buf[(fila - ry) * rw + (c - rx)] = color;
            a = finTramo;
        }
    }
}

// Arma la zona (fondo + sprites) en un lienzo en memoria y la manda de una
// sola vez a la pantalla: sin parpadeo. Uso:
//   GFXcanvas16* lienzo = empezarZona(...);  dibujar en *lienzo;  terminarZona(lienzo, x, y);
GFXcanvas16* empezarZona(const uint16_t* colores, const uint16_t* conteos, int runs, int ancho,
                         int rx, int ry, int rw, int rh, float factor) {
    GFXcanvas16* lienzo = new GFXcanvas16(rw, rh);
    if (!lienzo->getBuffer()) { delete lienzo; return nullptr; }
    fondoRegionEnBuffer(colores, conteos, runs, ancho, rx, ry, rw, rh, factor, lienzo->getBuffer());
    return lienzo;
}

void terminarZona(GFXcanvas16* lienzo, int rx, int ry) {
    tft.drawRGBBitmap(rx, ry, lienzo->getBuffer(), lienzo->width(), lienzo->height());
    delete lienzo;
}

// ---------- Backend ----------
void consultarEstado() {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(String(urlBase) + "/estado");
    int codigo = http.GET();
    if (codigo > 0) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
            hambreActual = doc["hambre"];
            suenoActual = doc["sueno"];
            dopaminaActual = doc["dopamina"];
            durmiendoServidor = doc["durmiendo"] | false;
            statsNuevos = true;
        }
    }
    http.end();
}

void enviarAccionAhora(const char* tipo) {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(String(urlBase) + "/accion");
    http.addHeader("Content-Type", "application/json");
    http.POST(String("{\"tipo\":\"") + tipo + "\"}");
    http.end();
    consultarEstado();
}

// Desde las pantallas se usa ESTA: deja la accion en la cola y vuelve al instante.
// El servidor de Render (plan gratis) puede tardar hasta ~50 s en despertarse,
// y antes eso congelaba la pantalla hasta que respondia.
void enviarAccion(const char* tipo) {
    xQueueSend(colaAcciones, &tipo, 0);
}

// Tarea de red: corre sola en el nucleo 0, mientras el loop (nucleo 1) dibuja y lee botones.
// Manda las acciones pendientes y cada 1 minuto vuelve a pedir los stats.
// Las preguntas que se usan: las bajadas del servidor, o las fijas si no hubo internet
int cantPreguntas() { return preguntasListas ? nPreguntasBajadas : N_PREGUNTAS_FIJAS; }
const char* textoPregunta(int i) { return preguntasListas ? preguntasBajadas[i] : preguntasFijas[i]; }
const char* clavePregunta(int i) { return preguntasListas ? clavesBajadas[i] : clavesFijas[i]; }

// Baja la lista de preguntas (GET /preguntas). Se llama solo desde la tarea de red.
void descargarPreguntas() {
    if (preguntasListas || WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(String(urlBase) + "/preguntas");
    if (http.GET() == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
            int n = 0;
            for (JsonObject p : doc.as<JsonArray>()) {
                if (n >= MAX_PREGUNTAS) break;
                // las tildes se pasan a la tabla de la pantalla (ver aCp437)
                strlcpy(preguntasBajadas[n], aCp437(p["pregunta"] | "").c_str(), sizeof(preguntasBajadas[n]));
                strlcpy(clavesBajadas[n], p["clave"] | "", sizeof(clavesBajadas[n]));
                n++;
            }
            if (n > 0) {
                nPreguntasBajadas = n;
                preguntasListas = true;  // recien ahora el loop las empieza a usar
            }
        }
    }
    http.end();
}

void tareaRed(void*) {
    unsigned long inicioWiFi = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicioWiFi < 10000) vTaskDelay(pdMS_TO_TICKS(100));
    consultarEstado();
    descargarPreguntas();
    for (;;) {
        const char* tipo;
        if (xQueueReceive(colaAcciones, &tipo, pdMS_TO_TICKS(intervaloConsultaMs)) == pdTRUE) {
            enviarAccionAhora(tipo);
        } else {
            consultarEstado();
            descargarPreguntas();  // por si la primera vez no hubo internet
        }
    }
}

String obtenerRespuesta(const char* clave) {
    if (WiFi.status() != WL_CONNECTED) return "Sin WiFi";
    HTTPClient http;
    http.begin(String(urlBase) + "/respuesta/" + clave);
    int codigo = http.GET();
    String resultado = "(sin respuesta)";
    if (codigo > 0) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
            resultado = doc["respuesta"].as<String>();
        }
    }
    http.end();
    return resultado;
}

// ---------- Pantallas ----------
// Zona de la principal que se repinta al animar (Roberto + la estrella del festejo)
#define PRINCIPAL_ZONA_X 50
#define PRINCIPAL_ZONA_Y 150
#define PRINCIPAL_ZONA_W 170
#define PRINCIPAL_ZONA_H 140
#define ESTADO_BAJO 30   // debajo de esto Roberto muestra hambre / sueno / aburrimiento

// Que sprite toca ahora en la principal
const char* const* spritePrincipal() {
    unsigned long t = millis() - animInicio;
    if (animPrincipal == ANIM_SALUDO) return SPR_SALUDO[(t / 300) % N_SALUDO];
    if (animPrincipal == ANIM_FESTEJO) return SPR_FESTEJO[0];
    // reposo: si algun stat esta bajo, se muestra el MAS bajo de los tres
    int minimo = min(hambreActual, min(suenoActual, dopaminaActual));
    if (minimo < ESTADO_BAJO) {
        if (minimo == hambreActual) return SPR_HAMBRE[0];
        if (minimo == suenoActual) return SPR_BOSTEZO[0];
        return SPR_ABURRIDO[0];
    }
    // contento: parpadea un ratito cada 3 segundos
    return SPR_NORMAL[(millis() % 3000) < 250 ? 1 : 0];
}

// Arranca una animacion especial en la principal (saludo al prender, festejo al comer...)
void empezarAnimPrincipal(AnimPrincipal anim, unsigned long duracion) {
    animPrincipal = anim;
    animInicio = millis();
    animDuracion = duracion;
    if (anim == ANIM_SALUDO) sonar(SONIDO_SALUDO);
    if (anim == ANIM_FESTEJO) sonar(SONIDO_FESTEJO);
}

// La estrella del festejo titila de un lado al otro de Roberto: fase 0 / 1, o -1 = sin estrella
int faseEstrella() {
    if (animPrincipal != ANIM_FESTEJO) return -1;
    return ((millis() - animInicio) / 250) % 2;
}

void dibujarZonaPrincipal(const char* const* sprite, int fase) {
    GFXcanvas16* lienzo = empezarZona(FONDO_COLOR, FONDO_CONTEO, FONDO_RUNS, FONDO_ANCHO,
                                      PRINCIPAL_ZONA_X, PRINCIPAL_ZONA_Y, PRINCIPAL_ZONA_W, PRINCIPAL_ZONA_H, 1.0);
    if (!lienzo) return;
    dibujarSpriteEn(*lienzo, ROBERTO_X - PRINCIPAL_ZONA_X, ROBERTO_Y - PRINCIPAL_ZONA_Y, sprite,
                    PALETA_LETRAS, PALETA_COLORES, PALETA_N, 5);
    if (fase >= 0) {
        int ex = (fase == 0) ? 100 : 0;
        int ey = (fase == 0) ? 10 : 25;
        dibujarSpriteEn(*lienzo, ex, ey, spr_estrellafestejo, PALETA_ESTRELLAFESTEJO,
                        PALETA_ESTRELLAFESTEJO_COLORES, PALETA_ESTRELLAFESTEJO_N, 2);
    }
    terminarZona(lienzo, PRINCIPAL_ZONA_X, PRINCIPAL_ZONA_Y);
}

// Se llama en cada vuelta del loop: repinta solo si cambio el sprite o la estrella
void animarPrincipal() {
    if (animPrincipal != ANIM_REPOSO && millis() - animInicio > animDuracion) animPrincipal = ANIM_REPOSO;
    const char* const* sprite = spritePrincipal();
    int fase = faseEstrella();
    if (sprite != spriteDibujado || fase != faseEstrellaDibujada) {
        spriteDibujado = sprite;
        faseEstrellaDibujada = fase;
        dibujarZonaPrincipal(sprite, fase);
    }
}

void dibujarPrincipal() {
    dibujarFondo();
    spriteDibujado = spritePrincipal();
    faseEstrellaDibujada = faseEstrella();
    dibujarZonaPrincipal(spriteDibujado, faseEstrellaDibujada);
}

// Los circulos e iconos ya vienen dibujados en fondo_menu.h.
// El seleccionado lleva una flechita blanca abajo, apuntando hacia arriba.
// Para borrarla se repinta esa zona del fondo.
#define MENU_RADIO_V 33     // medio alto de los circulos (agrandados)
#define FLECHA_W 16
#define FLECHA_H 11
#define FLECHA_SEP 4        // espacio entre el circulo y la punta de la flecha

void dibujarCirculoMenu(int i) {
    int zx = menuX[i] - FLECHA_W / 2 - 1;
    int zy = menuY[i] + MENU_RADIO_V + FLECHA_SEP;
    int zw = FLECHA_W + 3;
    int zh = FLECHA_H + 2;
    GFXcanvas16* lienzo = empezarZona(FONDO_MENU_COLOR, FONDO_MENU_CONTEO, FONDO_MENU_RUNS, FONDO_MENU_ANCHO,
                                      zx, zy, zw, zh, 1.0);
    if (!lienzo) return;
    if (i == cursorMenu) {
        int punta = zw / 2;
        uint16_t borde = tft.color565(235, 152, 102);  // el naranja del dibujo, para que se vea sobre el crema
        lienzo->fillTriangle(punta, 0, 1, FLECHA_H, zw - 2, FLECHA_H, ILI9341_WHITE);
        lienzo->drawTriangle(punta, 0, 1, FLECHA_H, zw - 2, FLECHA_H, borde);
    }
    terminarZona(lienzo, zx, zy);
}

// Intentaron dormirlo con el sueno lleno: el circulo de dormir "parpadea" dos veces
// con un sonidito suave, y se queda en el menu. Sin carteles ni retos.
#define MENU_RADIO_H 36     // medio ancho de los circulos (agrandados)

void pintarCirculoMenu(int i, float factor) {
    int zx = menuX[i] - MENU_RADIO_H - 1;
    int zy = menuY[i] - MENU_RADIO_V - 1;
    GFXcanvas16* lienzo = empezarZona(FONDO_MENU_COLOR, FONDO_MENU_CONTEO, FONDO_MENU_RUNS, FONDO_MENU_ANCHO,
                                      zx, zy, 2 * MENU_RADIO_H + 2, 2 * MENU_RADIO_V + 2, factor);
    if (lienzo) terminarZona(lienzo, zx, zy);
}

void noTieneSueno() {
    sonar(SONIDO_NO);
    for (int vez = 0; vez < 2; vez++) {
        pintarCirculoMenu(cursorMenu, 0.75);
        delay(120);
        pintarCirculoMenu(cursorMenu, 1.0);
        delay(120);
    }
}

// Barras de stats: el marco y los iconos estan en el fondo, aca solo se pinta el relleno.
#define BARRA_X 32
#define BARRA_W 194
#define BARRA_H 9
const int barraY[3] = {236, 267, 298};

void dibujarBarraStat(int fila, int valor) {
    uint16_t colorLleno = tft.color565(235, 152, 102);   // el naranja del dibujo
    uint16_t colorVacio = tft.color565(248, 231, 121);   // el amarillo de adentro de la barra
    int lleno = BARRA_W * constrain(valor, 0, 100) / 100;
    tft.fillRect(BARRA_X, barraY[fila], lleno, BARRA_H, colorLleno);
    tft.fillRect(BARRA_X + lleno, barraY[fila], BARRA_W - lleno, BARRA_H, colorVacio);
}

void dibujarStatsMenu() {
    dibujarBarraStat(0, hambreActual);
    dibujarBarraStat(1, suenoActual);
    dibujarBarraStat(2, dopaminaActual);
}

void dibujarMenu() {
    dibujarFondoRLE(FONDO_MENU_COLOR, FONDO_MENU_CONTEO, FONDO_MENU_RUNS, FONDO_MENU_ANCHO, FONDO_MENU_ALTO, 1.0);
    dibujarCirculoMenu(cursorMenu);
    dibujarStatsMenu();
}

// Pantalla de comida (fondo_comida.h): una comida a la vez, grande en el recuadro,
// con su nombre abajo. NAV cambia de comida, SELECT es el boton "Comer".
#define CARTA_X 59        // zona que se repinta: recuadro + plaquita del nombre
#define CARTA_Y 45
#define CARTA_W 122
#define CARTA_H 145
#define COMIDA_GRANDE_ESCALA 4
#define PLACA_W 84
#define PLACA_Y 164       // plaquita del nombre (mismo estilo que el boton Comer)
#define PLACA_H 22
#define BOTON_COMER_X 148
#define BOTON_COMER_Y 196
#define BOTON_COMER_W 70
#define BOTON_COMER_H 17

void dibujarCartaComida() {
    GFXcanvas16* lienzo = empezarZona(FONDO_COMIDA_COLOR, FONDO_COMIDA_CONTEO, FONDO_COMIDA_RUNS, FONDO_COMIDA_ANCHO,
                                      CARTA_X, CARTA_Y, CARTA_W, CARTA_H, 1.0);
    if (!lienzo) return;
    uint16_t naranja = tft.color565(235, 152, 102);
    uint16_t amarillo = tft.color565(248, 231, 121);
    uint16_t borde = tft.color565(235, 203, 118);

    // comida centrada en el recuadro (24 px * escala 4 = 96 px)
    int lado = 24 * COMIDA_GRANDE_ESCALA;
    int sx = (CARTA_W - lado) / 2;
    int sy = 8;
    if (cursorComida == 0) dibujarSpriteEn(*lienzo, sx, sy, SPR_LECHE, PALETA_LECHE, PALETA_LECHE_COLORES, PALETA_LECHE_N, COMIDA_GRANDE_ESCALA);
    else dibujarSpriteEn(*lienzo, sx, sy, SPR_TORTA, PALETA_TORTA, PALETA_TORTA_COLORES, PALETA_TORTA_N, COMIDA_GRANDE_ESCALA);

    // plaquita con el nombre
    int px = (CARTA_W - PLACA_W) / 2;
    int py = PLACA_Y - CARTA_Y;
    lienzo->fillRect(px, py, PLACA_W, PLACA_H, borde);
    lienzo->fillRect(px + 2, py + 2, PLACA_W - 4, PLACA_H - 4, amarillo);
    const char* nombre = comidaItems[cursorComida];
    lienzo->setTextSize(2);
    lienzo->setTextColor(naranja);
    lienzo->setCursor((CARTA_W - (int)strlen(nombre) * 12) / 2 + 1, py + 4);
    lienzo->print(nombre);

    terminarZona(lienzo, CARTA_X, CARTA_Y);
}

// Flecha a la derecha del recuadro: indica que con NAV se cambia de comida
#define FLECHA_COMIDA_X 190
#define FLECHA_COMIDA_Y 101   // centro vertical del recuadro

void dibujarFlechaComida() {
    uint16_t naranja = tft.color565(235, 152, 102);
    tft.fillTriangle(FLECHA_COMIDA_X, FLECHA_COMIDA_Y - 12, FLECHA_COMIDA_X, FLECHA_COMIDA_Y + 12,
                     FLECHA_COMIDA_X + 14, FLECHA_COMIDA_Y, ILI9341_WHITE);
    tft.drawTriangle(FLECHA_COMIDA_X, FLECHA_COMIDA_Y - 12, FLECHA_COMIDA_X, FLECHA_COMIDA_Y + 12,
                     FLECHA_COMIDA_X + 14, FLECHA_COMIDA_Y, naranja);
}

// Efecto de "boton apretado": el boton Comer se oscurece un instante
void apretarBotonComer() {
    GFXcanvas16* lienzo = empezarZona(FONDO_COMIDA_COLOR, FONDO_COMIDA_CONTEO, FONDO_COMIDA_RUNS, FONDO_COMIDA_ANCHO,
                                      BOTON_COMER_X, BOTON_COMER_Y, BOTON_COMER_W, BOTON_COMER_H, 0.8);
    if (!lienzo) return;
    terminarZona(lienzo, BOTON_COMER_X, BOTON_COMER_Y);
    delay(150);
}

void dibujarComida() {
    dibujarFondoRLE(FONDO_COMIDA_COLOR, FONDO_COMIDA_CONTEO, FONDO_COMIDA_RUNS, FONDO_COMIDA_ANCHO, FONDO_COMIDA_ALTO, 1.0);
    dibujarCartaComida();
    dibujarFlechaComida();
}

// Redibuja solo la zona de Roberto + comida. mordidas: 0 = comida entera,
// MORDIDAS = ya no queda nada. Se "come" desde el lado de Roberto (derecha).
void dibujarZonaCocina(int frame, int mordidas) {
    GFXcanvas16* lienzo = empezarZona(FONDO_COCINA_COLOR, FONDO_COCINA_CONTEO, FONDO_COCINA_RUNS, FONDO_COCINA_ANCHO,
                                      COCINA_ZONA_X, COCINA_ZONA_Y, COCINA_ZONA_W, COCINA_ZONA_H, 1.0);
    if (!lienzo) return;
    int cx = COMIDA_MESA_X - COCINA_ZONA_X;
    int cy = COMIDA_MESA_Y - COCINA_ZONA_Y;
    int colMax = 24 - (24 * mordidas) / MORDIDAS;
    if (colMax > 0) {
        if (comidaElegida == 0) dibujarSpriteEn(*lienzo, cx, cy, SPR_LECHE, PALETA_LECHE, PALETA_LECHE_COLORES, PALETA_LECHE_N, COMIDA_ESCALA, 1.0, colMax);
        else dibujarSpriteEn(*lienzo, cx, cy, SPR_TORTA, PALETA_TORTA, PALETA_TORTA_COLORES, PALETA_TORTA_N, COMIDA_ESCALA, 1.0, colMax);
    }
    dibujarSpriteEn(*lienzo, ROBERTO_COCINA_X - COCINA_ZONA_X, ROBERTO_COCINA_Y - COCINA_ZONA_Y, SPR_COMER[frame],
                    PALETA_LETRAS, PALETA_COLORES, PALETA_N, ROBERTO_COCINA_ESCALA);
    terminarZona(lienzo, COCINA_ZONA_X, COCINA_ZONA_Y);
}

void dibujarCocina() {
    cocinaInicio = millis();
    frameCocinaDibujado = 0;
    mordidasDibujadas = 0;
    dibujarFondoCocina();
    dibujarZonaCocina(0, 0);
}

#define DORMIR_ESCALA 6
#define DORMIR_LADO (24 * DORMIR_ESCALA)

void dibujarZonaDormir() {
    GFXcanvas16* lienzo = empezarZona(FONDO_COLOR, FONDO_CONTEO, FONDO_RUNS, FONDO_ANCHO,
                                      ROBERTO_DORMIR_X, ROBERTO_DORMIR_Y, DORMIR_LADO, DORMIR_LADO, 0.5);
    if (!lienzo) return;
    dibujarSpriteEn(*lienzo, 0, 0, SPR_DORMIR[frameDormir], PALETA_LETRAS, PALETA_COLORES, PALETA_N, DORMIR_ESCALA, 0.5);
    terminarZona(lienzo, ROBERTO_DORMIR_X, ROBERTO_DORMIR_Y);
}

// Barrita de sueno abajo de todo mientras duerme (se ve como va subiendo)
#define SUENO_BARRA_X 40
#define SUENO_BARRA_Y 300
#define SUENO_BARRA_W 160
#define SUENO_BARRA_H 10

void dibujarBarraSueno() {
    uint16_t borde = tft.color565(150, 150, 200);
    uint16_t lleno = tft.color565(170, 160, 230);
    uint16_t vacio = tft.color565(40, 40, 70);
    int ancho = (SUENO_BARRA_W - 4) * constrain((int)suenoActual, 0, 100) / 100;
    tft.drawRect(SUENO_BARRA_X, SUENO_BARRA_Y, SUENO_BARRA_W, SUENO_BARRA_H, borde);
    tft.fillRect(SUENO_BARRA_X + 2, SUENO_BARRA_Y + 2, ancho, SUENO_BARRA_H - 4, lleno);
    tft.fillRect(SUENO_BARRA_X + 2 + ancho, SUENO_BARRA_Y + 2, SUENO_BARRA_W - 4 - ancho, SUENO_BARRA_H - 4, vacio);
}

void dibujarDormir() {
    sonar(SONIDO_DORMIR);  // cancion de cuna cortita, bajando
    intervaloConsultaMs = 10000;  // mientras duerme, mirar el sueno cada 10 s
    frameDormir = 0;
    ultimoCambioDormir = millis();
    dibujarFondo(0.5);
    dibujarZonaDormir();
    dibujarBarraSueno();
}

// Se despierta (porque tocaron un boton o porque ya descanso todo)
void despertar() {
    intervaloConsultaMs = 60000;
    durmiendoServidor = false;
    enviarAccion("despertar");
    empezarAnimPrincipal(ANIM_SALUDO, 1500);  // se despierta saludando
    irA(P_PRINCIPAL);
}

// Renglones de fondo_info.h (medidos sobre el dibujo): parte de adentro, en y
const int renglonY0[6] = {40, 84, 134, 174, 224, 264};
const int renglonY1[6] = {63, 106, 153, 196, 246, 286};
#define RENGLON_X 14
#define RENGLON_W 210

// Repinta un renglon: fondo + texto, y la flechita si es el elegido
void dibujarRenglonInfo(int i) {
    int alto = renglonY1[i] - renglonY0[i] + 1;
    GFXcanvas16* lienzo = empezarZona(FONDO_INFO_COLOR, FONDO_INFO_CONTEO, FONDO_INFO_RUNS, FONDO_INFO_ANCHO,
                                      RENGLON_X, renglonY0[i], RENGLON_W, alto, 1.0);
    if (!lienzo) return;
    uint16_t naranja = tft.color565(235, 152, 102);
    int medio = alto / 2;
    if (i == cursorInfo) {
        lienzo->fillTriangle(6, medio - 7, 6, medio + 7, 17, medio, ILI9341_WHITE);
        lienzo->drawTriangle(6, medio - 7, 6, medio + 7, 17, medio, naranja);
    }
    if (i < cantPreguntas()) {
        // letra grande si entra en el renglon; si no, chica
        lienzo->cp437(true);
        lienzo->setTextSize(strlen(textoPregunta(i)) <= 15 ? 2 : 1);
        lienzo->setTextColor(naranja);
        lienzo->setCursor(24, strlen(textoPregunta(i)) <= 15 ? medio - 7 : medio - 3);
        lienzo->print(textoPregunta(i));
    }
    terminarZona(lienzo, RENGLON_X, renglonY0[i]);
}

void dibujarInfoMenu() {
    dibujarFondoRLE(FONDO_INFO_COLOR, FONDO_INFO_CONTEO, FONDO_INFO_RUNS, FONDO_INFO_ANCHO, FONDO_INFO_ALTO, 1.0);
    // titulo en la plaquita de arriba (x 52..188, y 4..32)
    const char* titulo = "PREGUNTAS";
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(235, 152, 102));
    tft.setCursor(120 - (strlen(titulo) * 12 - 2) / 2, 11);
    tft.print(titulo);
    infoDibujadaConBajadas = preguntasListas;
    if (cursorInfo >= cantPreguntas()) cursorInfo = 0;
    for (int i = 0; i < cantPreguntas(); i++) dibujarRenglonInfo(i);
}

// ---------- Info: Roberto contesta con una burbuja de dialogo ----------
#define BURBUJA_X 20
#define BURBUJA_Y 0
#define BURBUJA_ESCALA 8
// Rectangulo de adentro de la burbuja donde entra el texto (medido sobre el dibujo)
#define TEXTO_X 56
#define TEXTO_Y 58
#define TEXTO_W 136
#define TEXTO_H 68
// Zona de Roberto en esta pantalla (no pisa la colita de la burbuja)
#define HABLAR_ZONA_X 60
#define HABLAR_ZONA_Y 190
#define HABLAR_ZONA_W 120
#define HABLAR_ZONA_H 100
#define MAX_LINEAS 8

// La fuente de la pantalla no entiende UTF-8 (lo que manda el servidor).
// Se pasan las letras con tilde y la ñ a la tabla "CP437", que si las tiene.
String aCp437(const String& s) {
    String r;
    for (unsigned int i = 0; i < s.length(); i++) {
        uint8_t c = s[i];
        if ((c == 0xC3 || c == 0xC2) && i + 1 < s.length()) {
            uint8_t d = s[++i];
            if (c == 0xC2) { if (d == 0xBF) r += (char)0xA8; else if (d == 0xA1) r += (char)0xAD; continue; }  // ¿ ¡
            switch (d) {
                case 0xA1: r += (char)0xA0; break;  // á
                case 0xA9: r += (char)0x82; break;  // é
                case 0xAD: r += (char)0xA1; break;  // í
                case 0xB3: r += (char)0xA2; break;  // ó
                case 0xBA: r += (char)0xA3; break;  // ú
                case 0xBC: r += (char)0x81; break;  // ü
                case 0xB1: r += (char)0xA4; break;  // ñ
                case 0x91: r += (char)0xA5; break;  // Ñ
                case 0x89: r += (char)0x90; break;  // É
                case 0x81: r += 'A'; break;
                case 0x8D: r += 'I'; break;
                case 0x93: r += 'O'; break;
                case 0x9A: r += 'U'; break;
            }
        } else if (c < 0x80) {
            r += (char)c;
        }
    }
    return r;
}

// Corta el texto en renglones de hasta maxLetras, sin partir palabras (salvo que no entren)
int partirEnLineas(const String& texto, int maxLetras, String* lineas, int maxLineas) {
    int n = 0;
    String actual = "";
    int i = 0;
    while (i < (int)texto.length() && n < maxLineas) {
        int fin = texto.indexOf(' ', i);
        if (fin < 0) fin = texto.length();
        String palabra = texto.substring(i, fin);
        i = fin + 1;
        while ((int)palabra.length() > maxLetras) {  // palabra larguisima: se corta
            if (actual.length() > 0 && n < maxLineas) { lineas[n++] = actual; actual = ""; }
            if (n < maxLineas) lineas[n++] = palabra.substring(0, maxLetras);
            palabra = palabra.substring(maxLetras);
        }
        String prueba = actual.length() ? actual + " " + palabra : palabra;
        if ((int)prueba.length() <= maxLetras) {
            actual = prueba;
        } else {
            if (n < maxLineas) lineas[n++] = actual;
            actual = palabra;
        }
    }
    if (actual.length() > 0 && n < maxLineas) lineas[n++] = actual;
    return n;
}

void dibujarBurbuja(const String& texto) {
    dibujarSpriteGenerico(BURBUJA_X, BURBUJA_Y, spr_burbujatexto, PALETA_BURBUJATEXTO,
                          PALETA_BURBUJATEXTO_COLORES, PALETA_BURBUJATEXTO_N, BURBUJA_ESCALA);
    String t = aCp437(texto);
    String lineas[MAX_LINEAS];
    int n = 0;
    int tam = 2;
    // letra grande si entra; si no, letra chica
    for (tam = 2; tam >= 1; tam--) {
        n = partirEnLineas(t, TEXTO_W / (6 * tam), lineas, MAX_LINEAS);
        if (n * (8 * tam + 2) <= TEXTO_H || tam == 1) break;
    }
    int alto = 8 * tam + 2;
    int y = TEXTO_Y + (TEXTO_H - n * alto) / 2;
    tft.cp437(true);
    tft.setTextSize(tam);
    tft.setTextColor(PALETA_COLORES[0]);  // el gris oscuro del contorno de Roberto
    for (int i = 0; i < n; i++) {
        int ancho = lineas[i].length() * 6 * tam;
        tft.setCursor(TEXTO_X + (TEXTO_W - ancho) / 2, y + i * alto);
        tft.print(lineas[i]);
    }
}

// Roberto hablando: abre y cierra la boca mientras dura la respuesta, despues sonrie
int frameHablar() {
    if (millis() - hablarInicio > hablarDuracion) return 0;
    return ((millis() - hablarInicio) / 180) % 2 == 0 ? 1 : 0;  // boca abierta / cerrada
}

void dibujarZonaHablar(int frame) {
    GFXcanvas16* lienzo = empezarZona(FONDO_COLOR, FONDO_CONTEO, FONDO_RUNS, FONDO_ANCHO,
                                      HABLAR_ZONA_X, HABLAR_ZONA_Y, HABLAR_ZONA_W, HABLAR_ZONA_H, 1.0);
    if (!lienzo) return;
    dibujarSpriteEn(*lienzo, ROBERTO_X - HABLAR_ZONA_X, ROBERTO_Y - HABLAR_ZONA_Y, SPR_HABLAR[frame],
                    PALETA_LETRAS, PALETA_COLORES, PALETA_N, 5);
    terminarZona(lienzo, HABLAR_ZONA_X, HABLAR_ZONA_Y);
}

// Cuando llega la respuesta: se escribe y Roberto "la dice"
void decirRespuesta(const String& texto) {
    respuestaActual = texto;
    dibujarBurbuja(texto);
    hablarInicio = millis();
    hablarDuracion = constrain(texto.length() * 80, 600, 3000);
}

void dibujarInfoRespuesta() {
    dibujarFondo();
    dibujarBurbuja(respuestaActual);
    frameHablarDibujado = frameHablar();
    dibujarZonaHablar(frameHablarDibujado);
}

void dibujarJugar() {
    tft.fillScreen(ILI9341_BLACK);
    dibujarSprite(70, 110, SPR_JUGAR[0]);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(15, 230);
    tft.print("Proximamente...");
}

void irA(int nueva) {
    pantallaActual = static_cast<Pantalla>(nueva);
    switch (nueva) {
        case P_PRINCIPAL: dibujarPrincipal(); break;
        case P_MENU: dibujarMenu(); break;
        case P_COMIDA: dibujarComida(); break;
        case P_COCINA: dibujarCocina(); break;
        case P_DORMIR: dibujarDormir(); break;
        case P_INFO: dibujarInfoMenu(); break;
        case P_INFO_RESPUESTA: dibujarInfoRespuesta(); break;
        case P_JUGAR: dibujarJugar(); break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    // Si la placa se reinicio sola, aca dice por que (se ve en el monitor serie)
    Serial.printf("Arranque. Motivo del ultimo reinicio: %d\n", (int)esp_reset_reason());

    pinMode(PIN_NAV, INPUT_PULLUP);
    pinMode(PIN_SELECT, INPUT_PULLUP);
    pinMode(PIN_BACK, INPUT_PULLUP);

    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    tft.begin();
    tft.setRotation(0);

    tft.fillScreen(ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(10, 10);
    tft.println("Iniciando Roberto...");

    empezarAnimPrincipal(ANIM_SALUDO, 2500);  // Roberto saluda al prender
    irA(P_PRINCIPAL);
    WiFi.begin(ssid, password);

    // La red corre aparte (nucleo 0): Roberto ya se puede usar mientras conecta.
    // Prioridad 0 (la mas baja): el cifrado HTTPS tarda varios segundos y si no
    // deja descansar al nucleo 0, el "watchdog" cree que se colgo y reinicia la placa.
    colaAcciones = xQueueCreate(8, sizeof(const char*));
    xTaskCreatePinnedToCore(tareaRed, "red", 16384, nullptr, tskIDLE_PRIORITY, nullptr, 0);
}

void loop() {
    bool nav = sePresiono(PIN_NAV, navAnterior);
    bool sel = sePresiono(PIN_SELECT, selectAnterior);
    bool back = sePresiono(PIN_BACK, backAnterior);
    if (nav || sel || back) sonar(SONIDO_BOTON);  // "clic" suave en cada boton

    // Llegaron stats nuevos del servidor: actualizar lo que se ve
    if (statsNuevos) {
        statsNuevos = false;
        if (pantallaActual == P_MENU) dibujarStatsMenu();
        if (pantallaActual == P_DORMIR) dibujarBarraSueno();
        // si la placa se reinicio mientras Roberto dormia, vuelve a la cama
        if (primeraConsulta) {
            primeraConsulta = false;
            if (durmiendoServidor && suenoActual < 100 && pantallaActual == P_PRINCIPAL) irA(P_DORMIR);
        }
        // en la principal no hace falta: animarPrincipal() ya elige el sprite segun los stats
    }

    switch (pantallaActual) {
        case P_PRINCIPAL:
            animarPrincipal();
            if (sel) irA(P_MENU);
            break;

        case P_MENU:
            if (nav) {
                int anterior = cursorMenu;
                cursorMenu = (cursorMenu + 1) % 4;
                dibujarCirculoMenu(anterior);
                dibujarCirculoMenu(cursorMenu);
            }
            if (back) irA(P_PRINCIPAL);
            if (sel) {
                if (cursorMenu == 0) irA(P_COMIDA);
                else if (cursorMenu == 1) {
                    if (suenoActual >= 100) {
                        noTieneSueno();
                    } else {
                        enviarAccion("dormir");
                        irA(P_DORMIR);
                    }
                }
                else if (cursorMenu == 2) irA(P_JUGAR);
                else irA(P_INFO);
            }
            break;

        case P_COMIDA:
            if (nav) { cursorComida = (cursorComida + 1) % 2; dibujarCartaComida(); }
            if (back) irA(P_MENU);
            if (sel) {
                apretarBotonComer();
                comidaElegida = cursorComida;
                irA(P_COCINA);
            }
            break;

        case P_COCINA: {
            unsigned long pasado = millis() - cocinaInicio;
            int frame = pasado / 300 % N_COMER;
            // la ultima mordida llega un poco antes del final, para que se vea el plato vacio
            int mordidas = min((int)(pasado * (MORDIDAS + 1) / DURACION_CINEMATICA), MORDIDAS);
            // solo se repinta cuando algo cambia (antes era la pantalla entera cada 50 ms)
            if (frame != frameCocinaDibujado || mordidas != mordidasDibujadas) {
                if (mordidas != mordidasDibujadas) sonar(SONIDO_MORDIDA);
                frameCocinaDibujado = frame;
                mordidasDibujadas = mordidas;
                dibujarZonaCocina(frame, mordidas);
            }

            if (pasado > DURACION_CINEMATICA) {
                enviarAccion("alimentar");
                empezarAnimPrincipal(ANIM_FESTEJO, 1500);
                irA(P_PRINCIPAL);
            }
            break;
        }

        case P_DORMIR:
            if (millis() - ultimoCambioDormir > 600) {
                frameDormir = (frameDormir + 1) % N_DORMIR;
                ultimoCambioDormir = millis();
                dibujarZonaDormir();
            }
            // cualquier boton lo despierta, y se despierta solo cuando el sueno llega a 100
            if (sel || nav || back || suenoActual >= 100) despertar();
            break;

        case P_JUGAR:
            if (back || sel) irA(P_MENU);
            break;

        case P_INFO:
            // si las preguntas del servidor llegaron con la pantalla abierta, se redibuja
            if (preguntasListas != infoDibujadaConBajadas) dibujarInfoMenu();
            if (nav) {
                int anterior = cursorInfo;
                cursorInfo = (cursorInfo + 1) % cantPreguntas();
                dibujarRenglonInfo(anterior);
                dibujarRenglonInfo(cursorInfo);
            }
            if (back) irA(P_MENU);
            if (sel) {
                // primero Roberto con la burbuja "..." (pensando), despues la respuesta
                respuestaActual = "...";
                hablarDuracion = 0;
                irA(P_INFO_RESPUESTA);
                decirRespuesta(obtenerRespuesta(clavePregunta(cursorInfo)));
            }
            break;

        case P_INFO_RESPUESTA: {
            int frame = frameHablar();
            if (frame != frameHablarDibujado) {
                if (frame == 1) sonar(SONIDO_HABLAR);  // "bip" cada vez que abre la boca
                frameHablarDibujado = frame;
                dibujarZonaHablar(frame);
            }
            if (back || sel) irA(P_INFO);
            break;
        }
    }

    delay(50);
}