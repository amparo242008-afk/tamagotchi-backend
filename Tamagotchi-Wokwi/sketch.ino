// =====================================================================
//  ROBERTO - firmware del tamagotchi (ESP32-S3 + pantalla ILI9341)
// =====================================================================
// Como esta armado este archivo (en orden, de arriba hacia abajo):
//
//   1. Librerias y dibujos (.h)        - lo que se usa de afuera
//   2. Configuracion                   - WiFi, servidor, pines, posiciones
//   3. Variables globales              - stats, agenda, pantallas, cursores
//   4. Sonido y reloj                  - melodias del buzzer, hora real (NTP)
//   5. Botones                         - sePresiono()
//   6. Dibujo                          - colores, sprites, fondos comprimidos (RLE)
//   7. Backend (internet)              - hablar con el servidor de Render
//   8. Una seccion por pantalla        - Principal, Menu, Comida, Cocina, Dormir,
//                                        Info, Minijuego, Agenda (avisos)
//   9. irA()                           - cambia de pantalla
//  10. setup() y loop()                - el arranque y la vuelta infinita
//
// Como funciona todo junto:
//   - setup() corre UNA vez al prender: prepara la pantalla y los botones,
//     muestra la Principal y lanza la "tarea de red".
//   - loop() se repite para siempre (unas 20 veces por segundo): lee los botones
//     y, segun la pantalla en la que estamos, decide que hacer.
//   - La tarea de red (tareaRed) corre AL MISMO TIEMPO en el otro nucleo del
//     ESP32: habla con el servidor sin trabar la pantalla.
//   - Roberto funciona como una "maquina de estados": siempre esta en UNA
//     pantalla (pantallaActual) y irA() lo pasa a otra.
// =====================================================================

// ---------- 1. Librerias ----------
#include <WiFi.h>               // conectarse a internet
#include <HTTPClient.h>         // pedirle cosas al servidor (GET / POST)
#include <ArduinoJson.h>        // leer las respuestas del servidor (vienen en JSON)
#include <SPI.h>                // el "cable" de comunicacion con la pantalla
#include <Adafruit_GFX.h>       // funciones de dibujo (rectangulos, texto, triangulos...)
#include <Adafruit_ILI9341.h>   // el controlador de nuestra pantalla en particular
// Los dibujos de Piskel ya convertidos a codigo (ver herramientas/)
#include "sprites/sprites.h"
#include "sprites/comida.h"
#include "sprites/iconos.h"
#include "fondos/fondo.h"
#include "fondos/cocina.h"
#include "fondos/fondo_menu.h"
#include "fondos/fondo_comida.h"
#include "sprites/efectos.h"
#include "fondos/fondo_info.h"
#include "fondos/parque.h"
#include "sprites/ObjetosJuego.h"
#include "fondos/bano.h"
#include "fondos/bienvenida.h"

// ---------- 2. Configuracion ----------
// Red WiFi. "Wokwi-GUEST" es la red falsa del simulador; con la placa real
// aca va el nombre y la contrasena del WiFi de la casa.
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Direccion del servidor (server.js, subido a Render). Todas las consultas empiezan con esto.
const char* urlBase = "https://tamagotchi-api-9vk6.onrender.com";

// Pines de la pantalla (SPI de hardware: el ESP32 tiene un circuito propio para
// mandar datos rapido a la pantalla; por software era lento y trababa los botones).
// MISO no se usa (la pantalla no le contesta al ESP32), pero hay que darle un pin libre.
#define TFT_CS 10
#define TFT_DC 13
#define TFT_RST 14
#define TFT_MOSI 11
#define TFT_SCK 12
#define TFT_MISO 16
// "tft" es la pantalla: todo lo que se dibuja pasa por aca (tft.fillRect, tft.print...)
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Los 3 botones y el buzzer. #define = un nombre fijo para un numero (no ocupa memoria).
// NAV = moverse / cambiar opcion, SELECT = elegir, BACK = volver.
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

#define DURACION_CINEMATICA 3000   // cuanto dura la escena de la cocina (en milisegundos)
#define MORDIDAS 3                 // en cuantas mordidas desaparece la comida

// =====================================================================
// 3. VARIABLES GLOBALES (las puede usar cualquier parte del programa)
// =====================================================================

// ---------- Stats de Roberto (0 a 100) ----------
// Los calcula el servidor; aca guardamos la ultima copia que nos mando.
// volatile: los actualiza la tarea de red (otro nucleo) y los lee el loop.
// Esa palabra le avisa al compilador que el valor puede cambiar "desde afuera"
// y que siempre lo lea de nuevo de la memoria.
volatile int hambreActual = 100;
volatile int suenoActual = 100;
volatile int dopaminaActual = 100;
volatile bool statsNuevos = false;           // "llegaron stats, actualiza la pantalla" (lo baja el loop)
volatile bool durmiendoServidor = false;      // el servidor dice que Roberto esta dormido
volatile bool primeraConsulta = true;         // para retomar el sueno si se reinicio la placa
bool statsRecibidos = false;                  // ya llego al menos una vez la respuesta del servidor
volatile uint32_t intervaloConsultaMs = 60000;  // cada cuanto se piden los stats (mas seguido al dormir)

// Cola de acciones para mandar al servidor sin trabar la pantalla.
// Funciona como una fila de espera: las pantallas dejan un "Pedido" (ej. "alimentar")
// y siguen de largo; la tarea de red los va sacando de a uno y los manda al servidor.
QueueHandle_t colaAcciones;
// Lo que viaja por la cola: una accion del juguete ("alimentar", "jugar"...) o un
// registro de la agenda ("registro"). dato = puntos del juego, o numero de rango.
struct Pedido { const char* tipo; int dato; const char* respuesta; };

// ---------- Agenda de Roberto (rutina.json en el servidor) ----------
// En cada rango horario Roberto pregunta "¿ya comiste?" (o te banaste, o a dormir).
// "Si" = la persona confirma, y Roberto lo hace con ella. "Mas tarde" = vuelve a
// preguntar despues. Lo que no se contesta se anota en silencio para el acompanante.
#define MAX_RANGOS 8
// enum = una lista de opciones con nombre (por dentro son 0, 1, 2...)
enum Actividad { ACT_COMIDA, ACT_HIGIENE, ACT_DORMIR };
// struct = una "ficha" que junta varios datos. Cada rango de rutina.json
// (desayuno, almuerzo, cena, bano, dormir) tiene su ficha, con lo que Roberto
// necesita recordar de ese rango HOY.
struct Rango {
    char clave[16];
    char pregunta[48];           // lo que dice Roberto, ej. "¿Ya almorzaste?" (de rutina.json)
    Actividad actividad;
    int desde, hasta;            // minutos del dia (ej. 8:30 = 510); hasta < desde = cruza la medianoche
    bool confirmado;             // ya dijo "si" en esta vuelta del rango
    int avisos;                  // cuantas veces pregunto en esta vuelta
    unsigned long ultimoAvisoMs; // cuando pregunto la ultima vez (millis = ms desde que prendio)
    bool cerrado;               // ya se anoto "sin respuesta"
    long vuelta;                 // identifica la vuelta (el dia) del rango, para arrancar de cero
};
Rango rangos[MAX_RANGOS];       // las fichas de todos los rangos
int nRangos = 0;                // cuantos rangos hay de verdad (los que mando el servidor)
// Estos valores se pisan con los de rutina.json cuando llega la agenda:
int reinsistirMin = 45;         // cada cuanto vuelve a preguntar
int maxAvisos = 3;              // cuantas veces como mucho por rango
int silencioDesde = 22 * 60;     // de noche Roberto no hace ruido
int silencioHasta = 8 * 60;
volatile bool rutinaLista = false;
unsigned long ultimoAvisoGlobal = 0;  // para no encadenar dos preguntas seguidas
unsigned long ultimaRevisionAgenda = 0;
int rangoAviso = 0;              // de que rango es la pregunta en pantalla
int cursorAviso = 0;             // 0 = Si, 1 = Mas tarde
unsigned long avisoInicio = 0;

// ---------- Pantallas ----------
// Todas las pantallas que existen. pantallaActual dice en cual estamos;
// para cambiar se usa irA(P_...), nunca se cambia pantallaActual a mano.
enum Pantalla { P_PRINCIPAL, P_MENU, P_COMIDA, P_COCINA, P_DORMIR, P_INFO, P_INFO_RESPUESTA, P_JUGAR, P_JUGAR_FIN, P_AVISO,
                P_BIENVENIDA, P_BANO };
Pantalla pantallaActual = P_BIENVENIDA;

// Los "cursor..." guardan que opcion esta elegida en cada pantalla (0 = la primera)
int cursorMenu = 0;
// Arriba las tres necesidades (comida, bano, dormir), abajo jugar e info
#define N_MENU 5
enum OpcionMenu { MENU_COMIDA, MENU_BANO, MENU_DORMIR, MENU_JUGAR, MENU_INFO };
// Centros de los circulos de fondo_menu.h (medidos sobre el dibujo)
const int menuX[N_MENU] = {40, 120, 200, 80, 160};
const int menuY[N_MENU] = {49, 49, 49, 144, 144};

int cursorComida = 0;
int comidaElegida = 0;  // copia de cursorComida al entrar a la cocina
// Cada comida: su nombre, su dibujo y su paleta (estan en sprites/comida.h).
// Para sumar una comida nueva alcanza con agregar una fila aca.
struct Comida { const char* nombre; const char* const* sprite; const char* letras; const uint16_t* colores; int n; };
const Comida comidas[] = {
    {"Leche", SPR_LECHE, PALETA_LECHE, PALETA_LECHE_COLORES, PALETA_LECHE_N},
    {"Torta", SPR_TORTA, PALETA_TORTA, PALETA_TORTA_COLORES, PALETA_TORTA_N},
    {"Gaseosa", SPR_GASEOSA, PALETA_GASEOSA, PALETA_GASEOSA_COLORES, PALETA_GASEOSA_N},
    {"Hamburguesa", SPR_HAMBURGUESA, PALETA_HAMBURGUESA, PALETA_HAMBURGUESA_COLORES, PALETA_HAMBURGUESA_N},
};
const int N_COMIDAS = sizeof(comidas) / sizeof(comidas[0]);  // cuantas filas tiene la tabla

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

String respuestaActual = "";    // el texto que Roberto esta diciendo en la burbuja

// Cocina: cuando empezo la escena y que se dibujo por ultima vez (para no repintar de mas)
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

// Dormir: que dibujo de la animacion toca (0 o 1) y cuando cambio por ultima vez
int frameDormir = 0;
unsigned long ultimoCambioDormir = 0;

// Como estaba cada boton en la vuelta anterior del loop (ver sePresiono)
bool navAnterior = false, selectAnterior = false, backAnterior = false;

// =====================================================================
// 4. SONIDO Y RELOJ
// =====================================================================

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
const Nota SONIDO_CORAZON[]  = {{1319, 40}, {1760, 60}, {0, 0}};
const Nota SONIDO_BOMBA[]    = {{220, 120}, {0, 0}};
const Nota SONIDO_AVISO[]    = {{MI5, 100}, {0, 60}, {SOL5, 100}, {0, 60}, {DO6, 180}, {0, 0}};
const Nota SONIDO_NO[]       = {{MI5, 80}, {0, 40}, {DO5, 120}, {0, 0}};
const Nota SONIDO_BURBUJA[]  = {{1200, 15}, {1700, 25}, {0, 0}};

// ---------- Reloj (hora real por internet, NTP) ----------
// El ESP32 no tiene pila de reloj: al prender no sabe que hora es.
// La tarea de red le pregunta la hora a un servidor de internet (NTP) y desde ahi la lleva sola.
bool horaLista() { return time(nullptr) > 1700000000; }  // antes de sincronizar, el reloj arranca en 1970

// La hora de ahora contada en minutos desde la medianoche (ej. 13:15 = 795).
// Asi comparar horarios es comparar numeros.
int minutoDelDia() {
    time_t t = time(nullptr);
    struct tm hora;
    localtime_r(&t, &hora);
    return hora.tm_hour * 60 + hora.tm_min;
}

// ¿El minuto m esta dentro del rango? (puede cruzar la medianoche, ej. 21:00 a 01:00)
bool enRango(int desde, int hasta, int m) {
    if (desde < hasta) return m >= desde && m < hasta;
    return m >= desde || m < hasta;
}

bool enSilencio() { return horaLista() && enRango(silencioDesde, silencioHasta, minutoDelDia()); }

// tone() del ESP32 tiene su propia cola: las notas se mandan todas juntas y suenan
// una detras de otra solas, sin trabar la pantalla ni los botones.
// De noche (horario de silencio de rutina.json) solo suena el clic de los botones.
void sonar(const Nota* melodia) {
    if (!SONIDO_ACTIVADO) return;
    if (melodia != SONIDO_BOTON && enSilencio()) return;
    for (int i = 0; melodia[i].ms > 0; i++) tone(PIN_BUZZER, melodia[i].freq, melodia[i].ms);
}

// =====================================================================
// 5. BOTONES
// =====================================================================
// Los botones estan con INPUT_PULLUP: sueltos leen HIGH, apretados leen LOW.
// sePresiono() devuelve true SOLO en el instante en que se aprieta (el "flanco"),
// no todo el tiempo que se mantiene apretado. Si no, una sola apretada
// contaria como 20 (una por cada vuelta del loop).
// "anterior" se pasa con & para que la funcion pueda guardar el estado nuevo.
bool sePresiono(int pin, bool &anterior) {
    bool actual = (digitalRead(pin) == LOW);
    bool disparo = (actual && !anterior);
    anterior = actual;
    return disparo;
}

// =====================================================================
// 6. DIBUJO
// =====================================================================
// Colores: la pantalla usa "RGB565", cada color entra en 16 bits:
// 5 bits de rojo, 6 de verde y 5 de azul. tft.color565(r, g, b) convierte
// un color normal (0-255 cada uno, como en Piskel) a ese formato.

// ---------- Color ----------
// Los sprites son filas de letras (ej. "..AAB.."); cada letra es un color de la paleta.
// Esta funcion busca que color le corresponde a una letra.
uint16_t colorDeLetraPaleta(char c, const char* letras, const uint16_t* colores, int n) {
    for (int i = 0; i < n; i++) if (letras[i] == c) return colores[i];
    return ILI9341_BLACK;
}

// Oscurece un color RGB565 multiplicando cada canal por un factor (0.5 = mitad de brillo)
// Se usa para el "filtro nocturno" de Dormir y para el boton apretado.
// Los >> y & separan el rojo, el verde y el azul que vienen pegados en un solo numero,
// y el << del final los vuelve a pegar.
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
// Recorre el sprite letra por letra. Los '.' se saltean (transparente: se ve lo que
// ya habia atras). Cada letra se dibuja como un cuadrado de "escala" x "escala" pixeles,
// asi un sprite de 24x24 con escala 5 ocupa 120x120 en la pantalla.
// "g" puede ser la pantalla (tft) o un lienzo en memoria (GFXcanvas16).
// colMax: solo dibuja las columnas 0..colMax-1 (sirve para "morder" la comida).
// lado: 24 para Roberto y la comida, 16 para el corazon y la bomba.
// espejo: lo dibuja dado vuelta (mirando para el otro lado).
void dibujarSpriteEn(Adafruit_GFX &g, int x, int y, const char* const* sprite,
                     const char* letras, const uint16_t* colores, int n, int escala,
                     float factor = 1.0, int colMax = 24, int lado = 24, bool espejo = false) {
    for (int fy = 0; fy < lado; fy++) {
        for (int fx = 0; fx < min(colMax, lado); fx++) {
            char c = sprite[fy][fx];
            if (c == '.') continue;
            uint16_t color = colorDeLetraPaleta(c, letras, colores, n);
            if (factor < 0.999) color = oscurecer(color, factor);
            int px = espejo ? (lado - 1 - fx) : fx;
            g.fillRect(x + px * escala, y + fy * escala, escala, escala, color);
        }
    }
}

// Atajos: dibujar directo en la pantalla (Generico) o un sprite de Roberto con su paleta (dibujarSprite)
void dibujarSpriteGenerico(int x, int y, const char* const* sprite,
                           const char* letras, const uint16_t* colores, int n, int escala, float factor = 1.0) {
    dibujarSpriteEn(tft, x, y, sprite, letras, colores, n, escala, factor);
}

void dibujarSprite(int x, int y, const char* const* sprite, int escala = 5, float factor = 1.0) {
    dibujarSpriteGenerico(x, y, sprite, PALETA_LETRAS, PALETA_COLORES, PALETA_N, escala, factor);
}

// Iconos de un solo color: 'X' = pintar, cualquier otra cosa = transparente
void dibujarIcono(int x, int y, const char* const* icono, uint16_t colorTinta) {
    for (int fy = 0; fy < 24; fy++) {
        for (int fx = 0; fx < 24; fx++) {
            if (icono[fy][fx] == 'X') tft.drawPixel(x + fx, y + fy, colorTinta);
        }
    }
}

// ---------- Fondos (RLE) ----------
// Un fondo de 240x320 son 76.800 pixeles: guardado tal cual no entra comodo.
// Por eso esta comprimido con RLE: en vez de "rojo, rojo, rojo, rojo" se guarda
// "rojo x4". colores[i] es el color y conteos[i] cuantos pixeles seguidos lo tienen.
// Los arrays viven en PROGMEM (la memoria flash, la grande); pgm_read_word los lee de ahi.
// Se dibujan de izquierda a derecha y de arriba hacia abajo, como se lee un libro.
// factor < 1 oscurece todo el fondo (noche).
void dibujarFondoRLE(const uint16_t* colores, const uint16_t* conteos, int runs, int ancho, int alto, float factor) {
    tft.startWrite();
    tft.setAddrWindow(0, 0, ancho, alto);  // "voy a pintar este rectangulo, en orden"
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
                         int rx, int ry, int rw, int rh, float factor, uint16_t* buf,
                         int runInicio = 0, uint32_t posInicio = 0) {
    // runInicio/posInicio: si se sabe en que run empieza la fila ry, se arranca
    // directo de ahi (ver indice del parque en el minijuego). Si no, desde el principio.
    uint32_t pos = posInicio;
    uint32_t inicio = (uint32_t)ry * ancho;
    uint32_t fin = (uint32_t)(ry + rh) * ancho;
    for (int i = runInicio; i < runs && pos < fin; i++) {
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
// sola vez a la pantalla: sin parpadeo. (GFXcanvas16 = una "pantalla de mentira"
// en la memoria RAM; se dibuja ahi con las mismas funciones y despues se copia.) Uso:
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

// =====================================================================
// 7. BACKEND (hablar con el servidor de Render)
// =====================================================================
// Todas estas funciones hacen lo mismo: arman la direccion (urlBase + "/algo"),
// piden con GET (traer datos) o POST (mandar datos), y leen la respuesta JSON.
// OJO: las que terminan en "Ahora" y las "descargar..." pueden tardar hasta ~50 s,
// asi que solo se llaman desde la tarea de red, nunca desde el loop.

// GET /estado: trae hambre, sueno, dopamina y si esta durmiendo
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

// POST /accion: avisa que Roberto comio / jugo / se durmio / se desperto.
// Manda algo como {"tipo":"jugar","puntos":7} y despues pide los stats nuevos.
void enviarAccionAhora(const char* tipo, int puntos) {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(String(urlBase) + "/accion");
    http.addHeader("Content-Type", "application/json");
    String cuerpo = String("{\"tipo\":\"") + tipo + "\"";
    if (strcmp(tipo, "jugar") == 0) cuerpo += ",\"puntos\":" + String(puntos);
    http.POST(cuerpo + "}");
    http.end();
    consultarEstado();
}

// Desde las pantallas se usa ESTA: deja la accion en la cola y vuelve al instante.
// El servidor de Render (plan gratis) puede tardar hasta ~50 s en despertarse,
// y antes eso congelaba la pantalla hasta que respondia.
void enviarAccion(const char* tipo, int puntos = 0) {
    Pedido p = {tipo, puntos, nullptr};
    xQueueSend(colaAcciones, &p, 0);
}

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

// "08:30" -> 510 minutos
int aMinutos(const char* hhmm) {
    return atoi(hhmm) * 60 + atoi(hhmm + 3);
}

// Identifica la vuelta mas reciente del rango (cuando empezo, en minutos desde 1970).
// Cuando cambia, empezo un rango nuevo: Roberto arranca de cero.
long vueltaDeRango(const Rango& r) {
    long ahora = time(nullptr) / 60;
    return ahora - ((minutoDelDia() - r.desde + 1440) % 1440);
}

// Baja la agenda (GET /rutina). Necesita la hora real para saber en que vuelta esta cada rango.
void descargarRutina() {
    if (rutinaLista || WiFi.status() != WL_CONNECTED || !horaLista()) return;
    HTTPClient http;
    http.begin(String(urlBase) + "/rutina");
    if (http.GET() == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
            reinsistirMin = doc["reinsistir_min"] | 45;
            maxAvisos = doc["max_avisos"] | 3;
            silencioDesde = aMinutos(doc["silencio"]["desde"] | "22:00");
            silencioHasta = aMinutos(doc["silencio"]["hasta"] | "08:00");
            int n = 0;
            for (JsonObject r : doc["rangos"].as<JsonArray>()) {
                if (n >= MAX_RANGOS) break;
                Rango& rango = rangos[n];
                strlcpy(rango.clave, r["clave"] | "", sizeof(rango.clave));
                strlcpy(rango.pregunta, r["pregunta"] | "", sizeof(rango.pregunta));
                const char* act = r["actividad"] | "comida";
                rango.actividad = strcmp(act, "dormir") == 0 ? ACT_DORMIR
                                : strcmp(act, "higiene") == 0 ? ACT_HIGIENE : ACT_COMIDA;
                rango.desde = aMinutos(r["desde"] | "00:00");
                rango.hasta = aMinutos(r["hasta"] | "00:00");
                rango.confirmado = r["confirmado"] | false;  // si ya dijo "si" antes de reiniciar
                // si ya pregunto antes de reiniciarse, sigue la cuenta (y respeta los 45 min)
                rango.avisos = r["avisos"] | 0;
                rango.ultimoAvisoMs = 0;
                if (!r["min_desde_ultimo"].isNull())
                    rango.ultimoAvisoMs = millis() - (unsigned long)(r["min_desde_ultimo"] | 0) * 60000UL;
                rango.cerrado = false;
                rango.vuelta = vueltaDeRango(rango);
                n++;
            }
            nRangos = n;
            rutinaLista = true;  // recien ahora el loop la empieza a usar
            Serial.printf("Agenda: %d rangos\n", n);
        }
    }
    http.end();
}

void enviarRegistroAhora(int rango, const char* respuesta) {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin(String(urlBase) + "/registro");
    http.addHeader("Content-Type", "application/json");
    http.POST(String("{\"clave\":\"") + rangos[rango].clave + "\",\"respuesta\":\"" + respuesta + "\"}");
    http.end();
}

// Desde las pantallas: anota que Roberto pregunto ("aviso") o la respuesta de la persona (si / mas_tarde / sin_respuesta)
void enviarRegistro(int rango, const char* respuesta) {
    Pedido p = {"registro", rango, respuesta};
    xQueueSend(colaAcciones, &p, 0);
}

// Tarea de red: corre sola en el nucleo 0, mientras el loop (nucleo 1) dibuja y lee botones.
// 1) espera el WiFi (hasta 10 s), 2) pide la hora, 3) baja stats, preguntas y agenda,
// 4) despues se queda para siempre: si llega un pedido a la cola lo manda;
//    si pasa un rato sin pedidos (intervaloConsultaMs), vuelve a pedir los stats.
void tareaRed(void*) {
    unsigned long inicioWiFi = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicioWiFi < 10000) vTaskDelay(pdMS_TO_TICKS(100));
    // Hora de Argentina (UTC-3) desde internet
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");
    for (int i = 0; i < 50 && !horaLista(); i++) vTaskDelay(pdMS_TO_TICKS(100));
    if (horaLista()) Serial.printf("Hora: %02d:%02d\n", minutoDelDia() / 60, minutoDelDia() % 60);
    consultarEstado();
    descargarPreguntas();
    descargarRutina();
    for (;;) {
        Pedido p;
        if (xQueueReceive(colaAcciones, &p, pdMS_TO_TICKS(intervaloConsultaMs)) == pdTRUE) {
            if (strcmp(p.tipo, "registro") == 0) enviarRegistroAhora(p.dato, p.respuesta);
            else enviarAccionAhora(p.tipo, p.dato);
        } else {
            consultarEstado();
            descargarPreguntas();  // por si la primera vez no hubo internet
            descargarRutina();
        }
    }
}

// GET /respuesta/<clave>: la respuesta de Roberto a una pregunta de Info.
// Esta SI se llama desde el loop (la pantalla espera con la burbuja "...").
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

// =====================================================================
// 8. PANTALLAS
// =====================================================================
// Cada pantalla tiene una funcion dibujarXxx() que la pinta entera al entrar
// (la llama irA), y a veces funciones chicas que repintan solo un pedacito
// cuando algo cambia (el cursor, una animacion). Lo que pasa con los botones
// en cada pantalla esta en loop(), en el switch de abajo de todo.

// ---------- Principal (el living con Roberto) ----------
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

// Pinta Roberto (y la estrella si festeja) sobre su pedacito de fondo.
// Dentro del lienzo las coordenadas arrancan en 0, por eso se resta PRINCIPAL_ZONA_X/Y.
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

// Al entrar a la Principal: fondo completo + Roberto
void dibujarPrincipal() {
    dibujarFondo();
    spriteDibujado = spritePrincipal();
    faseEstrellaDibujada = faseEstrella();
    dibujarZonaPrincipal(spriteDibujado, faseEstrellaDibujada);
}

// ---------- Menu (4 circulos + barras de stats) ----------
// Los circulos e iconos ya vienen dibujados en fondo_menu.h.
// El seleccionado lleva una flechita blanca abajo, apuntando hacia arriba.
// Para borrarla se repinta esa zona del fondo.
#define MENU_RADIO_V 33     // medio alto de los circulos (agrandados)
#define FLECHA_W 16
#define FLECHA_H 11
#define FLECHA_SEP 4        // espacio entre el circulo y la punta de la flecha

// Repinta la zona de la flechita del circulo i: con flecha si es el elegido, vacia si no
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

// fila: 0 = hambre, 1 = sueno, 2 = dopamina. La parte llena es proporcional al valor (0-100).
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

// ---------- Comida (elegir que come) ----------
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

// Repinta el recuadro con la comida elegida (cursorComida) y su nombre
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
    const Comida& c = comidas[cursorComida];
    dibujarSpriteEn(*lienzo, sx, sy, c.sprite, c.letras, c.colores, c.n, COMIDA_GRANDE_ESCALA);

    // plaquita con el nombre: se agranda si el nombre es largo (hasta el ancho del recuadro).
    // Letra grande = 12 px por letra; si ni asi entra (ej. "Hamburguesa"), letra chica (6 px).
    int tam = (int)strlen(c.nombre) * 12 <= CARTA_W - 8 ? 2 : 1;
    int anchoTexto = (int)strlen(c.nombre) * 6 * tam;
    int placaW = max(PLACA_W, min(anchoTexto + 12, CARTA_W));
    int px = (CARTA_W - placaW) / 2;
    int py = PLACA_Y - CARTA_Y;
    lienzo->fillRect(px, py, placaW, PLACA_H, borde);
    lienzo->fillRect(px + 2, py + 2, placaW - 4, PLACA_H - 4, amarillo);
    lienzo->setTextSize(tam);
    lienzo->setTextColor(naranja);
    lienzo->setCursor((CARTA_W - anchoTexto) / 2 + 1, tam == 2 ? py + 4 : py + 7);
    lienzo->print(c.nombre);

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

// ---------- Cocina (escena: Roberto come, sin botones) ----------
// Redibuja solo la zona de Roberto + comida. mordidas: 0 = comida entera,
// MORDIDAS = ya no queda nada. Se "come" desde el lado de Roberto (derecha).
void dibujarZonaCocina(int frame, int mordidas) {
    GFXcanvas16* lienzo = empezarZona(FONDO_COCINA_COLOR, FONDO_COCINA_CONTEO, FONDO_COCINA_RUNS, FONDO_COCINA_ANCHO,
                                      COCINA_ZONA_X, COCINA_ZONA_Y, COCINA_ZONA_W, COCINA_ZONA_H, 1.0);
    if (!lienzo) return;
    int cx = COMIDA_MESA_X - COCINA_ZONA_X;
    int cy = COMIDA_MESA_Y - COCINA_ZONA_Y;
    int colMax = 24 - (24 * mordidas) / MORDIDAS;  // cuantas columnas de la comida quedan
    if (colMax > 0) {
        const Comida& c = comidas[comidaElegida];
        dibujarSpriteEn(*lienzo, cx, cy, c.sprite, c.letras, c.colores, c.n, COMIDA_ESCALA, 1.0, colMax);
    }
    dibujarSpriteEn(*lienzo, ROBERTO_COCINA_X - COCINA_ZONA_X, ROBERTO_COCINA_Y - COCINA_ZONA_Y, SPR_COMER[frame],
                    PALETA_LETRAS, PALETA_COLORES, PALETA_N, ROBERTO_COCINA_ESCALA);
    terminarZona(lienzo, COCINA_ZONA_X, COCINA_ZONA_Y);
}

// Al entrar: anota la hora de inicio (el loop calcula todo a partir de ahi) y pinta la cocina
void dibujarCocina() {
    cocinaInicio = millis();
    frameCocinaDibujado = 0;
    mordidasDibujadas = 0;
    dibujarFondoCocina();
    dibujarZonaCocina(0, 0);
}

// ---------- Dormir (living oscurecido, Roberto grande con Zzz) ----------
#define DORMIR_ESCALA 6
#define DORMIR_LADO (24 * DORMIR_ESCALA)

// Repinta solo a Roberto durmiendo (todo con factor 0.5 = de noche)
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

// ---------- Info (lista de preguntas) ----------
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

// Dibuja la burbuja y el texto adentro, centrado. Prueba primero con letra
// grande (tam 2); si no entra, usa la chica (tam 1). Cada letra mide 6x8 * tam.
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
    hablarDuracion = constrain(texto.length() * 80, 600, 3000);  // mas texto = habla mas rato (entre 0,6 y 3 s)
}

void dibujarInfoRespuesta() {
    dibujarFondo();
    dibujarBurbuja(respuestaActual);
    frameHablarDibujado = frameHablar();
    dibujarZonaHablar(frameHablarDibujado);
}

// ---------- Minijuego "Atrapar" ----------
// Roberto corre por el parque atrapando corazones (+1) y esquivando bombas (-1).
// Boton izquierdo (NAV) = izquierda, boton derecho (SELECT) = derecha,
// boton del medio (BACK) = terminar. Sin vidas ni "game over": se juega lo que se quiera.
#define JUEGO_FRAME_MS 33          // ~30 cuadros por segundo
#define MAX_OBJETOS 6
#define OBJ_ESCALA 2
#define OBJ_LADO (16 * OBJ_ESCALA)
#define PROB_CORAZON 70            // de cada 100 cosas que caen, 70 son corazones
#define JUEGO_ROB_ESCALA 4
#define JUEGO_ROB_LADO (24 * JUEGO_ROB_ESCALA)
#define JUEGO_ROB_Y 208            // con los pies sobre el caminito
#define JUEGO_ROB_VEL 7            // pixeles por cuadro que avanza Roberto
#define JUEGO_ANIM_MS 90           // cada cuanto cambia el dibujo de correr (menos = piernas mas rapidas)
#define JUEGO_ESPERA_MIN 900       // tiempo entre una cosa que cae y la siguiente (ms)
#define JUEGO_ESPERA_MAX 1500
#define JUEGO_SEPARACION_X 70      // distancia minima (en x) con la cosa anterior
#define JUEGO_ROB_MIN_X (-20)      // el cuerpo ocupa las columnas 5..16 del sprite
#define JUEGO_ROB_MAX_X (240 - 68)
#define HUD_X 6                    // contador de corazones, arriba a la izquierda
#define HUD_Y 6
#define HUD_W 100
#define HUD_H 24

// Cada cosa que cae: si esta en uso, si es corazon (o bomba), donde esta y que tan rapido cae.
// Hay lugar para MAX_OBJETOS a la vez; cuando una se atrapa o sale de la pantalla, se libera.
struct Objeto { bool activo; bool corazon; int x; float y; float vel; };
Objeto objetos[MAX_OBJETOS];
int robX = 72;
bool robMiraDerecha = false;   // los sprites de correr miran a la izquierda
int robFrame = 0;
int corazones = 0;
unsigned long juegoUltimoFrame = 0;
unsigned long juegoProximoObjeto = 0;
int juegoUltimoX = -100;        // donde cayo la ultima cosa (para no tirar otra pegada)

// Indice del fondo del parque: en que run empieza cada fila. Asi se puede
// repintar un pedacito sin recorrer los ~14.000 runs desde el principio.
uint16_t parqueFilaRun[FONDO_PARQUE_ALTO];
uint32_t parqueFilaPos[FONDO_PARQUE_ALTO];
bool parqueIndexado = false;

void indexarParque() {
    if (parqueIndexado) return;
    uint32_t pos = 0;
    int fila = 0;
    for (int i = 0; i < FONDO_PARQUE_RUNS && fila < FONDO_PARQUE_ALTO; i++) {
        uint32_t fin = pos + pgm_read_word(&FONDO_PARQUE_CONTEO[i]);
        while (fila < FONDO_PARQUE_ALTO && (uint32_t)fila * FONDO_PARQUE_ANCHO < fin) {
            parqueFilaRun[fila] = i;
            parqueFilaPos[fila] = pos;
            fila++;
        }
        pos = fin;
    }
    parqueIndexado = true;
}

// ¿Se tocan dos rectangulos? (x, y, ancho, alto de cada uno)
// Sirve para saber si Roberto atrapo algo y que pedazo de pantalla hay que repintar.
bool seCruzan(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

// Repinta un rectangulo del juego con TODO lo que haya ahi (fondo, cosas que caen,
// Roberto y contador), armado en memoria: nada parpadea aunque se encimen.
void dibujarEscenaJuego(int rx, int ry, int rw, int rh) {
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > 240) rw = 240 - rx;
    if (ry + rh > 320) rh = 320 - ry;
    if (rw <= 0 || rh <= 0) return;
    GFXcanvas16* lienzo = new GFXcanvas16(rw, rh);
    if (!lienzo->getBuffer()) { delete lienzo; return; }
    fondoRegionEnBuffer(FONDO_PARQUE_COLOR, FONDO_PARQUE_CONTEO, FONDO_PARQUE_RUNS, FONDO_PARQUE_ANCHO,
                        rx, ry, rw, rh, 1.0, lienzo->getBuffer(), parqueFilaRun[ry], parqueFilaPos[ry]);

    for (int i = 0; i < MAX_OBJETOS; i++) {
        Objeto& o = objetos[i];
        if (!o.activo || !seCruzan(rx, ry, rw, rh, o.x, (int)o.y, OBJ_LADO, OBJ_LADO)) continue;
        if (o.corazon) dibujarSpriteEn(*lienzo, o.x - rx, (int)o.y - ry, SPR_CORAZON, PALETA_CORAZON,
                                       PALETA_CORAZON_COLORES, PALETA_CORAZON_N, OBJ_ESCALA, 1.0, 16, 16);
        else dibujarSpriteEn(*lienzo, o.x - rx, (int)o.y - ry, SPR_BOMBA, PALETA_BOMBA,
                             PALETA_BOMBA_COLORES, PALETA_BOMBA_N, OBJ_ESCALA, 1.0, 16, 16);
    }

    if (seCruzan(rx, ry, rw, rh, robX, JUEGO_ROB_Y, JUEGO_ROB_LADO, JUEGO_ROB_LADO)) {
        dibujarSpriteEn(*lienzo, robX - rx, JUEGO_ROB_Y - ry, SPR_CORRER[robFrame], PALETA_LETRAS, PALETA_COLORES,
                        PALETA_N, JUEGO_ROB_ESCALA, 1.0, 24, 24, robMiraDerecha);
    }

    if (seCruzan(rx, ry, rw, rh, HUD_X, HUD_Y, HUD_W, HUD_H)) {
        dibujarSpriteEn(*lienzo, HUD_X - rx, HUD_Y + 4 - ry, SPR_CORAZON, PALETA_CORAZON,
                        PALETA_CORAZON_COLORES, PALETA_CORAZON_N, 1, 1.0, 16, 16);
        String texto = "x " + String(corazones);
        lienzo->setTextSize(2);
        lienzo->setTextColor(PALETA_COLORES[0]);  // sombrita oscura para que se lea sobre el cielo
        lienzo->setCursor(HUD_X + 23 - rx, HUD_Y + 6 - ry);
        lienzo->print(texto);
        lienzo->setTextColor(ILI9341_WHITE);
        lienzo->setCursor(HUD_X + 22 - rx, HUD_Y + 5 - ry);
        lienzo->print(texto);
    }
    terminarZona(lienzo, rx, ry);
}

// Al entrar al juego: todo de cero (sin objetos, Roberto en el medio, 0 corazones)
void dibujarJugar() {
    indexarParque();
    randomSeed(micros());
    for (int i = 0; i < MAX_OBJETOS; i++) objetos[i].activo = false;
    robX = 72;
    robFrame = 0;
    robMiraDerecha = false;
    corazones = 0;
    juegoUltimoFrame = millis();
    juegoProximoObjeto = millis() + 800;
    dibujarFondoRLE(FONDO_PARQUE_COLOR, FONDO_PARQUE_CONTEO, FONDO_PARQUE_RUNS, FONDO_PARQUE_ANCHO, FONDO_PARQUE_ALTO, 1.0);
    dibujarEscenaJuego(robX, JUEGO_ROB_Y, JUEGO_ROB_LADO, JUEGO_ROB_LADO);
    dibujarEscenaJuego(HUD_X, HUD_Y, HUD_W, HUD_H);
}

// Un cuadro del juego: mover a Roberto, hacer caer las cosas, ver que atrapo
// Se llama ~30 veces por segundo.
void pasoJuego() {
    // aca NO se usa sePresiono: mientras se mantiene apretado, Roberto sigue corriendo
    bool izq = digitalRead(PIN_NAV) == LOW;
    bool der = digitalRead(PIN_SELECT) == LOW;

    // Roberto
    int viejoX = robX;
    int viejoFrame = robFrame;
    bool viejoMira = robMiraDerecha;
    if (izq && !der) { robX -= JUEGO_ROB_VEL; robMiraDerecha = false; }
    if (der && !izq) { robX += JUEGO_ROB_VEL; robMiraDerecha = true; }
    robX = constrain(robX, JUEGO_ROB_MIN_X, JUEGO_ROB_MAX_X);
    robFrame = (izq != der) ? (millis() / JUEGO_ANIM_MS) % N_CORRER : 0;
    if (robX != viejoX || robFrame != viejoFrame || robMiraDerecha != viejoMira) {
        // se repinta desde donde estaba hasta donde quedo (asi se borra el Roberto viejo)
        int x0 = min(robX, viejoX);
        int x1 = max(robX, viejoX) + JUEGO_ROB_LADO;
        dibujarEscenaJuego(x0, JUEGO_ROB_Y, x1 - x0, JUEGO_ROB_LADO);
    }

    // Lo que cae
    int cuerpoX = robX + 5 * JUEGO_ROB_ESCALA;   // la "caja" del cuerpo de Roberto
    int cuerpoY = JUEGO_ROB_Y + 6 * JUEGO_ROB_ESCALA;
    int cuerpoW = 12 * JUEGO_ROB_ESCALA;
    int cuerpoH = 16 * JUEGO_ROB_ESCALA;
    for (int i = 0; i < MAX_OBJETOS; i++) {
        Objeto& o = objetos[i];
        if (!o.activo) continue;
        int yViejo = (int)o.y;
        o.y += o.vel;  // cae un poquito
        // la caja del objeto se achica 4 px de cada lado, asi rozarlo con el borde no cuenta
        if (seCruzan(o.x + 4, (int)o.y + 4, OBJ_LADO - 8, OBJ_LADO - 8, cuerpoX, cuerpoY, cuerpoW, cuerpoH)) {
            o.activo = false;
            if (o.corazon) {
                corazones++;
                sonar(SONIDO_CORAZON);
            } else {
                if (corazones > 0) corazones--;  // nunca baja de 0
                sonar(SONIDO_BOMBA);
            }
            dibujarEscenaJuego(o.x, yViejo, OBJ_LADO, OBJ_LADO);  // borrarlo
            dibujarEscenaJuego(HUD_X, HUD_Y, HUD_W, HUD_H);       // actualizar el contador
        } else if (o.y > 320) {  // se cayo de la pantalla sin que lo agarre
            o.activo = false;
            dibujarEscenaJuego(o.x, yViejo, OBJ_LADO, OBJ_LADO);
        } else {  // sigue cayendo: repintar desde donde estaba hasta donde esta ahora
            dibujarEscenaJuego(o.x, yViejo, OBJ_LADO, (int)o.y - yViejo + OBJ_LADO);
        }
    }

    // Que caiga algo nuevo cada tanto (tranquilo, sin apuro)
    if (millis() >= juegoProximoObjeto) {
        for (int i = 0; i < MAX_OBJETOS; i++) {
            if (objetos[i].activo) continue;  // busca un lugar libre
            objetos[i].activo = true;
            objetos[i].corazon = random(100) < PROB_CORAZON;
            // que no caiga pegado al anterior: se sortea de nuevo hasta que quede lejos
            int x = random(0, 240 - OBJ_LADO);
            for (int intento = 0; intento < 8 && abs(x - juegoUltimoX) < JUEGO_SEPARACION_X; intento++)
                x = random(0, 240 - OBJ_LADO);
            objetos[i].x = x;
            juegoUltimoX = x;
            objetos[i].y = -OBJ_LADO;
            // todas caen casi igual de rapido: si no, las rapidas alcanzan a las lentas y se amontonan
            objetos[i].vel = 2.5 + random(0, 6) / 10.0;  // entre 2,5 y 3 pixeles por cuadro
            break;
        }
        juegoProximoObjeto = millis() + random(JUEGO_ESPERA_MIN, JUEGO_ESPERA_MAX);
    }
}

// Pantalla final: siempre en positivo, junte lo que junte
void dibujarJugarFin() {
    dibujarFondoRLE(FONDO_PARQUE_COLOR, FONDO_PARQUE_CONTEO, FONDO_PARQUE_RUNS, FONDO_PARQUE_ANCHO, FONDO_PARQUE_ALTO, 0.5);
    uint16_t borde = tft.color565(235, 203, 118);
    uint16_t amarillo = tft.color565(248, 231, 121);
    uint16_t naranja = tft.color565(235, 152, 102);
    // plaquita con el mismo estilo que los menus
    tft.fillRect(30, 50, 180, 110, borde);
    tft.fillRect(33, 53, 174, 104, amarillo);
    tft.cp437(true);
    tft.setTextSize(2);
    tft.setTextColor(naranja);
    const char* titulo = "\xAD" "Bien jugado!";  // \xAD = "¡" en la fuente de la pantalla
    tft.setCursor(120 - (strlen(titulo) * 12 - 2) / 2, 66);
    tft.print(titulo);
    dibujarSpriteEn(tft, 66, 98, SPR_CORAZON, PALETA_CORAZON, PALETA_CORAZON_COLORES, PALETA_CORAZON_N, 3, 1.0, 16, 16);
    tft.setTextSize(4);
    tft.setCursor(124, 108);
    tft.print("x" + String(corazones));
    dibujarSprite(60, 175, SPR_FESTEJO[0]);
    sonar(SONIDO_FESTEJO);
}

// ---------- Agenda: Roberto pregunta "¿ya comiste?" ----------
// Pantalla: el living, la burbuja con la pregunta (ej. "¿Ya almorzaste?"), Roberto abajo,
// y dos plaquitas: "Si" / "Mas tarde". Sin texto de reproche.
#define AVISO_OPC_Y 292
#define AVISO_OPC_H 24
#define AVISO_TIMEOUT_MS 60000UL   // si nadie contesta en 1 minuto, Roberto vuelve a lo suyo
const int avisoOpcX[2] = {20, 96};
const int avisoOpcW[2] = {66, 130};
const char* const avisoOpcTexto[2] = {"S\xA1", "M\xA0s tarde"};  // \xA1 = i con tilde, \xA0 = a con tilde

// Una plaquita ("Si" o "Mas tarde"), con flechita si es la elegida
void dibujarOpcionAviso(int i) {
    GFXcanvas16* lienzo = empezarZona(FONDO_COLOR, FONDO_CONTEO, FONDO_RUNS, FONDO_ANCHO,
                                      avisoOpcX[i], AVISO_OPC_Y, avisoOpcW[i], AVISO_OPC_H, 1.0);
    if (!lienzo) return;
    uint16_t borde = tft.color565(235, 203, 118);
    uint16_t amarillo = tft.color565(248, 231, 121);
    uint16_t naranja = tft.color565(235, 152, 102);
    int w = avisoOpcW[i];
    lienzo->fillRect(0, 0, w, AVISO_OPC_H, borde);
    lienzo->fillRect(2, 2, w - 4, AVISO_OPC_H - 4, amarillo);
    if (i == cursorAviso) {
        lienzo->fillTriangle(6, 5, 6, 18, 15, 11, ILI9341_WHITE);
        lienzo->drawTriangle(6, 5, 6, 18, 15, 11, naranja);
    }
    lienzo->cp437(true);
    lienzo->setTextSize(2);
    lienzo->setTextColor(naranja);
    lienzo->setCursor(20, 5);
    lienzo->print(avisoOpcTexto[i]);
    terminarZona(lienzo, avisoOpcX[i], AVISO_OPC_Y);
}

// Lo que pregunta Roberto: el texto de rutina.json, o uno general si no tiene
const char* textoAviso(const Rango& r) {
    if (r.pregunta[0]) return r.pregunta;
    if (r.actividad == ACT_COMIDA) return "¿Ya comiste?";
    if (r.actividad == ACT_DORMIR) return "¿Vamos a dormir?";
    return "¿Te bañaste?";
}

// Pantalla completa de la pregunta: Roberto la dice con la burbuja, como en Info
void dibujarAviso() {
    Rango& r = rangos[rangoAviso];  // & = "r" es el mismo rango, no una copia
    dibujarFondo();
    dibujarBurbuja(textoAviso(r));  // dibuja la burbuja y el texto centrado (pasa las tildes a la pantalla)
    for (int i = 0; i < 2; i++) dibujarOpcionAviso(i);
    hablarInicio = millis();
    hablarDuracion = 1200;
    frameHablarDibujado = frameHablar();
    dibujarZonaHablar(frameHablarDibujado);
}

// Roberto pregunta por el rango i: anota que pregunto y abre la pantalla del aviso
void mostrarAviso(int i) {
    rangoAviso = i;
    cursorAviso = 0;
    avisoInicio = millis();
    ultimoAvisoGlobal = millis();
    rangos[i].avisos++;
    rangos[i].ultimoAvisoMs = millis();
    enviarRegistro(i, "aviso");  // suma 1 en seguimiento_avisos
    sonar(SONIDO_AVISO);  // de noche no suena (ver enSilencio)
    irA(P_AVISO);
}

// "Si": la persona lo hizo. Roberto lo hace con ella.
void responderSi() {
    Rango& r = rangos[rangoAviso];
    r.confirmado = true;
    enviarRegistro(rangoAviso, "si");
    if (r.actividad == ACT_COMIDA) {
        irA(P_COMIDA);  // comen juntos: se elige la comida y Roberto va a la cocina
    } else if (r.actividad == ACT_DORMIR && suenoActual < 100) {
        enviarAccion("dormir");
        irA(P_DORMIR);
    } else if (r.actividad == ACT_HIGIENE) {
        irA(P_BANO);  // se banan juntos
    } else {
        empezarAnimPrincipal(ANIM_FESTEJO, 1500);
        irA(P_PRINCIPAL);
    }
}

// "Mas tarde": sin problema. Roberto saluda y vuelve a preguntar despues.
void responderMasTarde() {
    enviarRegistro(rangoAviso, "mas_tarde");
    empezarAnimPrincipal(ANIM_SALUDO, 1200);
    irA(P_PRINCIPAL);
}

// Se llama desde el loop una vez por segundo: ¿toca preguntar algo?
// Parte 1: pone al dia las fichas de todos los rangos (dia nuevo, rango terminado).
// Parte 2: si Roberto esta tranquilo, busca el primer rango que necesite preguntar.
void revisarAgenda() {
    if (!rutinaLista || !horaLista()) return;
    int m = minutoDelDia();
    for (int i = 0; i < nRangos; i++) {
        Rango& r = rangos[i];
        long v = vueltaDeRango(r);
        if (v != r.vuelta) {  // empezo una vuelta nueva del rango: cada dia arranca de cero
            r.vuelta = v;
            r.confirmado = false;
            r.avisos = 0;
            r.cerrado = false;
        }
        bool dentro = enRango(r.desde, r.hasta, m);
        // termino el rango, pregunto y no hubo "si": se anota en silencio para el acompanante
        if (!dentro && r.avisos > 0 && !r.confirmado && !r.cerrado) {
            r.cerrado = true;
            enviarRegistro(i, "sin_respuesta");
        }
    }
    // solo se pregunta con Roberto tranquilo en la pantalla principal
    if (pantallaActual != P_PRINCIPAL || animPrincipal != ANIM_REPOSO) return;
    // y nunca dos preguntas seguidas: al menos 2 minutos entre una y otra
    if (ultimoAvisoGlobal != 0 && millis() - ultimoAvisoGlobal < 2 * 60000UL) return;
    for (int i = 0; i < nRangos; i++) {
        Rango& r = rangos[i];
        // no pregunta si: ya dijo que si, ya pregunto el maximo, o no es el horario
        if (r.confirmado || r.avisos >= maxAvisos || !enRango(r.desde, r.hasta, m)) continue;
        // ni si todavia no pasaron los 45 minutos desde la ultima vez
        if (r.avisos > 0 && millis() - r.ultimoAvisoMs < (unsigned long)reinsistirMin * 60000UL) continue;
        mostrarAviso(i);
        return;
    }
}

// ---------- Bienvenida: Roberto saluda afuera y entra a la casa ----------
// Al prender, Roberto esta en la loma saludando. Con cualquier boton sale
// corriendo hacia la derecha (entra a la casa) y aparece el living.
#define BIENV_ROB_Y 150
#define BIENV_ROB_LADO (24 * 5)        // Roberto con escala 5 = 120 px
#define BIENV_ROB_VEL 8                // pixeles por cuadro al correr
#define BIENV_PLACA_X 60               // plaquita "Entrar" (mismo estilo que el aviso)
#define BIENV_PLACA_Y 284
#define BIENV_PLACA_W 120
#define BIENV_PLACA_H 24

bool bienvEntrando = false;            // false = saludando, true = corriendo hacia la casa
int bienvRobX = 60;
const char* const* bienvSpriteDibujado = nullptr;
int bienvFlechaDibujada = -1;
unsigned long bienvUltimoPaso = 0;

// Que dibujo de Roberto toca: saludando (quieto) o corriendo
const char* const* spriteBienvenida() {
    if (bienvEntrando) return SPR_CORRER[(millis() / 90) % N_CORRER];
    return SPR_SALUDO[(millis() / 300) % N_SALUDO];
}

// Repinta la franja de Roberto desde x0, de ancho w (recortada a la pantalla)
void dibujarZonaBienvenida(int x0, int w) {
    if (x0 < 0) { w += x0; x0 = 0; }
    if (x0 + w > 240) w = 240 - x0;
    if (w <= 0) return;
    GFXcanvas16* lienzo = empezarZona(FONDO_BIENVENIDA_COLOR, FONDO_BIENVENIDA_CONTEO, FONDO_BIENVENIDA_RUNS,
                                      FONDO_BIENVENIDA_ANCHO, x0, BIENV_ROB_Y, w, BIENV_ROB_LADO, 1.0);
    if (!lienzo) return;
    // los sprites de correr miran a la izquierda: espejo = true para que corra a la derecha
    dibujarSpriteEn(*lienzo, bienvRobX - x0, 0, bienvSpriteDibujado, PALETA_LETRAS, PALETA_COLORES,
                    PALETA_N, 5, 1.0, 24, 24, bienvEntrando);
    terminarZona(lienzo, x0, BIENV_ROB_Y);
}

// Plaquita "Entrar" con la flechita que titila (flecha = 1 visible, 0 apagada)
void dibujarPlacaBienvenida(int flecha) {
    GFXcanvas16* lienzo = empezarZona(FONDO_BIENVENIDA_COLOR, FONDO_BIENVENIDA_CONTEO, FONDO_BIENVENIDA_RUNS,
                                      FONDO_BIENVENIDA_ANCHO, BIENV_PLACA_X, BIENV_PLACA_Y, BIENV_PLACA_W, BIENV_PLACA_H, 1.0);
    if (!lienzo) return;
    uint16_t borde = tft.color565(235, 203, 118);
    uint16_t amarillo = tft.color565(248, 231, 121);
    uint16_t naranja = tft.color565(235, 152, 102);
    lienzo->fillRect(0, 0, BIENV_PLACA_W, BIENV_PLACA_H, borde);
    lienzo->fillRect(2, 2, BIENV_PLACA_W - 4, BIENV_PLACA_H - 4, amarillo);
    if (flecha) {
        lienzo->fillTriangle(14, 5, 14, 18, 23, 11, ILI9341_WHITE);
        lienzo->drawTriangle(14, 5, 14, 18, 23, 11, naranja);
    }
    lienzo->setTextSize(2);
    lienzo->setTextColor(naranja);
    lienzo->setCursor(32, 5);
    lienzo->print("Entrar");
    terminarZona(lienzo, BIENV_PLACA_X, BIENV_PLACA_Y);
}

void dibujarBienvenida() {
    bienvEntrando = false;
    bienvRobX = 60;
    dibujarFondoRLE(FONDO_BIENVENIDA_COLOR, FONDO_BIENVENIDA_CONTEO, FONDO_BIENVENIDA_RUNS,
                    FONDO_BIENVENIDA_ANCHO, FONDO_BIENVENIDA_ALTO, 1.0);
    bienvSpriteDibujado = spriteBienvenida();
    dibujarZonaBienvenida(bienvRobX, BIENV_ROB_LADO);
    bienvFlechaDibujada = 1;
    dibujarPlacaBienvenida(1);
    sonar(SONIDO_SALUDO);
}

// Se llama en cada vuelta del loop mientras estamos en la bienvenida
void animarBienvenida(bool boton) {
    if (!bienvEntrando) {
        // saludando: cambia el dibujo de la mano y titila la flecha de "Entrar"
        const char* const* sprite = spriteBienvenida();
        if (sprite != bienvSpriteDibujado) {
            bienvSpriteDibujado = sprite;
            dibujarZonaBienvenida(bienvRobX, BIENV_ROB_LADO);
        }
        int flecha = (millis() / 500) % 2;
        if (flecha != bienvFlechaDibujada) {
            bienvFlechaDibujada = flecha;
            dibujarPlacaBienvenida(flecha);
        }
        if (boton) {
            bienvEntrando = true;
            bienvUltimoPaso = millis();
        }
        return;
    }
    // corriendo: ~30 cuadros por segundo, hasta salir por la derecha
    if (millis() - bienvUltimoPaso < 33) return;
    bienvUltimoPaso = millis();
    int viejoX = bienvRobX;
    bienvRobX += BIENV_ROB_VEL;
    bienvSpriteDibujado = spriteBienvenida();
    // se repinta desde donde estaba hasta donde quedo (asi se borra el Roberto viejo)
    dibujarZonaBienvenida(viejoX, bienvRobX - viejoX + BIENV_ROB_LADO);
    if (bienvRobX > 240) irA(P_PRINCIPAL);  // ya entro: aparece el living
}

// ---------- Bano: Roberto se bana ----------
// Se llega desde el menu (circulo de la banadera) o desde el "Si" del aviso de higiene.
// Roberto (SPR_BANO, sin banadera propia) va metido en la banadera del fondo:
// alternando los dos dibujos (las gotitas de agua se mueven).
#define BANO_ROB_X 85                  // sentado adentro de la banadera del fondo
#define BANO_ROB_Y 100
#define BANO_ROB_ESCALA 5
#define BANO_ROB_LADO (24 * BANO_ROB_ESCALA)
#define BANO_FRAME_MS 400              // cada cuanto cambia el dibujo
#define BANO_DURACION 4000             // cuanto dura el bano (ms)

unsigned long banoInicio = 0;
int frameBanoDibujado = -1;

void dibujarZonaBano(int frame) {
    GFXcanvas16* lienzo = empezarZona(FONDO_BANO_COLOR, FONDO_BANO_CONTEO, FONDO_BANO_RUNS, FONDO_BANO_ANCHO,
                                      BANO_ROB_X, BANO_ROB_Y, BANO_ROB_LADO, BANO_ROB_LADO, 1.0);
    if (!lienzo) return;
    dibujarSpriteEn(*lienzo, 0, 0, SPR_BANO[frame], PALETA_BANO, PALETA_BANO_COLORES, PALETA_BANO_N, BANO_ROB_ESCALA);
    terminarZona(lienzo, BANO_ROB_X, BANO_ROB_Y);
}

void dibujarBano() {
    banoInicio = millis();
    frameBanoDibujado = 0;
    dibujarFondoRLE(FONDO_BANO_COLOR, FONDO_BANO_CONTEO, FONDO_BANO_RUNS, FONDO_BANO_ANCHO, FONDO_BANO_ALTO, 1.0);
    dibujarZonaBano(0);
    sonar(SONIDO_BURBUJA);
}

// =====================================================================
// 9. CAMBIAR DE PANTALLA
// =====================================================================
// Anota la pantalla nueva y la dibuja entera. Es el UNICO lugar donde se cambia
// pantallaActual, asi cada pantalla siempre arranca bien dibujada.
// Para sumar una pantalla nueva: agregarla al enum Pantalla, hacer su dibujarXxx(),
// agregar un case aca y otro en el switch del loop.
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
        case P_JUGAR_FIN: dibujarJugarFin(); break;
        case P_AVISO: dibujarAviso(); break;
        case P_BIENVENIDA: dibujarBienvenida(); break;
        case P_BANO: dibujarBano(); break;
    }
}

// =====================================================================
// 10. SETUP Y LOOP
// =====================================================================

// setup(): corre una sola vez, al prender la placa
void setup() {
    Serial.begin(115200);  // para mandar mensajes al "monitor serie" (sirve para buscar errores)
    delay(1000);
    // Si la placa se reinicio sola, aca dice por que (se ve en el monitor serie)
    Serial.printf("Arranque. Motivo del ultimo reinicio: %d\n", (int)esp_reset_reason());

    // Botones: INPUT_PULLUP usa una resistencia interna, asi no hacen falta resistencias en el circuito
    pinMode(PIN_NAV, INPUT_PULLUP);
    pinMode(PIN_SELECT, INPUT_PULLUP);
    pinMode(PIN_BACK, INPUT_PULLUP);

    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    tft.begin();
    tft.setRotation(0);  // vertical (240 de ancho x 320 de alto)

    tft.fillScreen(ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(10, 10);
    tft.println("Iniciando Roberto...");

    irA(P_BIENVENIDA);  // Roberto saluda afuera y espera que aprieten un boton para entrar
    WiFi.begin(ssid, password);  // empieza a conectar, pero no espera (eso lo hace la tarea de red)

    // La red corre aparte (nucleo 0): Roberto ya se puede usar mientras conecta.
    // Prioridad 0 (la mas baja): el cifrado HTTPS tarda varios segundos y si no
    // deja descansar al nucleo 0, el "watchdog" cree que se colgo y reinicia la placa.
    // Cola con lugar para 8 pedidos. Despues se lanza tareaRed: 16384 bytes de memoria
    // para ella, prioridad minima, en el nucleo 0.
    colaAcciones = xQueueCreate(8, sizeof(Pedido));
    xTaskCreatePinnedToCore(tareaRed, "red", 16384, nullptr, tskIDLE_PRIORITY, nullptr, 0);
}

// loop(): se repite para siempre. En cada vuelta:
//   1. lee los 3 botones (nav/sel/back = true solo si se acaban de apretar)
//   2. si llegaron stats nuevos, actualiza lo que se ve
//   3. una vez por segundo, revisa la agenda
//   4. segun la pantalla actual, anima y reacciona a los botones (el switch)
void loop() {
    bool nav = sePresiono(PIN_NAV, navAnterior);
    bool sel = sePresiono(PIN_SELECT, selectAnterior);
    bool back = sePresiono(PIN_BACK, backAnterior);
    if ((nav || sel || back) && pantallaActual != P_JUGAR) sonar(SONIDO_BOTON);  // "clic" suave (en el juego no)

    // Llegaron stats nuevos del servidor: actualizar lo que se ve
    if (statsNuevos) {
        statsNuevos = false;
        if (pantallaActual == P_MENU) dibujarStatsMenu();
        if (pantallaActual == P_DORMIR) dibujarBarraSueno();
        statsRecibidos = true;
        // en la principal no hace falta: animarPrincipal() ya elige el sprite segun los stats
    }
    // si la placa se reinicio mientras Roberto dormia, vuelve a la cama.
    // Se espera a que entre a la casa (si los stats llegaron durante la bienvenida, se revisa al entrar).
    if (primeraConsulta && statsRecibidos && pantallaActual == P_PRINCIPAL) {
        primeraConsulta = false;
        if (durmiendoServidor && suenoActual < 100) irA(P_DORMIR);
    }

    if (millis() - ultimaRevisionAgenda > 1000) {
        ultimaRevisionAgenda = millis();
        revisarAgenda();
    }

    // Que hacer segun la pantalla en la que estamos
    switch (pantallaActual) {
        case P_PRINCIPAL:
            animarPrincipal();
            if (sel) irA(P_MENU);
            break;

        case P_MENU:
            if (nav) {
                int anterior = cursorMenu;
                cursorMenu = (cursorMenu + 1) % N_MENU;  // % N_MENU: despues del ultimo vuelve al primero
                // se repintan solo los dos circulos que cambian: el que pierde la flecha y el que la gana
                dibujarCirculoMenu(anterior);
                dibujarCirculoMenu(cursorMenu);
            }
            if (back) irA(P_PRINCIPAL);
            if (sel) {
                if (cursorMenu == MENU_COMIDA) irA(P_COMIDA);
                else if (cursorMenu == MENU_BANO) irA(P_BANO);
                else if (cursorMenu == MENU_DORMIR) {
                    if (suenoActual >= 100) {
                        noTieneSueno();
                    } else {
                        enviarAccion("dormir");
                        irA(P_DORMIR);
                    }
                }
                else if (cursorMenu == MENU_JUGAR) irA(P_JUGAR);
                else irA(P_INFO);
            }
            break;

        case P_COMIDA:
            if (nav) { cursorComida = (cursorComida + 1) % N_COMIDAS; dibujarCartaComida(); }
            if (back) irA(P_MENU);
            if (sel) {
                apretarBotonComer();
                comidaElegida = cursorComida;
                irA(P_COCINA);
            }
            break;

        case P_COCINA: {
            unsigned long pasado = millis() - cocinaInicio;  // ms desde que empezo la escena
            int frame = pasado / 300 % N_COMER;               // cambia de dibujo cada 300 ms
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
                // la accion se manda recien al terminar de comer, no al elegir la comida
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
            if (millis() - juegoUltimoFrame >= JUEGO_FRAME_MS) {
                juegoUltimoFrame = millis();
                pasoJuego();
            }
            if (back) irA(P_JUGAR_FIN);
            break;

        case P_AVISO: {
            int frame = frameHablar();
            if (frame != frameHablarDibujado) {
                frameHablarDibujado = frame;
                dibujarZonaHablar(frame);
            }
            if (nav) {
                cursorAviso = 1 - cursorAviso;  // alterna entre 0 (Si) y 1 (Mas tarde)
                dibujarOpcionAviso(0);
                dibujarOpcionAviso(1);
            }
            if (sel) {
                if (cursorAviso == 0) responderSi();
                else responderMasTarde();
            } else if (back) {
                responderMasTarde();
            } else if (millis() - avisoInicio > AVISO_TIMEOUT_MS) {
                irA(P_PRINCIPAL);  // nadie contesto: Roberto vuelve a lo suyo, sin anotar nada
            }
            break;
        }

        case P_BIENVENIDA:
            animarBienvenida(sel || nav || back);
            break;

        case P_BANO: {
            int frame = (millis() - banoInicio) / BANO_FRAME_MS % N_BANO;
            if (frame != frameBanoDibujado) {
                frameBanoDibujado = frame;
                dibujarZonaBano(frame);
                sonar(SONIDO_BURBUJA);  // "chapoteo" cada vez que se mueve el agua
            }
            if (millis() - banoInicio > BANO_DURACION) {
                enviarAccion("banar");  // queda en el historial (como comer o dormir)
                empezarAnimPrincipal(ANIM_FESTEJO, 1500);  // sale del bano festejando
                irA(P_PRINCIPAL);
            }
            break;
        }

        case P_JUGAR_FIN:
            // cualquier boton: los corazones van a la dopamina y Roberto vuelve festejando
            if (sel || nav || back) {
                enviarAccion("jugar", corazones);
                empezarAnimPrincipal(ANIM_FESTEJO, 1500);
                irA(P_PRINCIPAL);
            }
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

    delay(pantallaActual == P_JUGAR ? 2 : 50);  // en el juego, el ritmo lo marca JUEGO_FRAME_MS
}