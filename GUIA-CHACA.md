# 📖 Guía de Chaca (para estudiar y arreglarme sola)

Esto es mi machete de cómo funciona Chaca por dentro: la placa, el código, el servidor, la base de datos y el panel. También tiene recetas para hacer cambios, qué hacer si algo falla y las preguntas que me puede hacer el jurado.

> **No hace falta saberse el código de memoria.** Lo importante es entender **qué hace cada parte y por qué**. Si me preguntan algo puntual, sé dónde buscarlo.

---

## 1. Chaca en una frase

**Chaca es una mascota virtual que acompaña la rutina diaria**: pregunta si comiste, si te lavaste, si vas a dormir, y propone una actividad por día. Mientras tanto, la persona que te acompaña (mi mamá, en la prueba) ve desde un panel web cómo venís, sin estar encima.

**No reemplaza la terapia ni a ningún profesional. Acompaña.**

### El mapa general

```
┌──────────────────────┐        ┌────────────────────┐        ┌───────────────┐
│  CHACA (la placa)    │  WiFi  │  SERVIDOR          │        │ BASE DE DATOS │
│  ESP32 + pantalla    │ ─────► │  (server.js)       │ ─────► │ (PostgreSQL)  │
│  + botones + buzzer  │ ◄───── │  en Render         │ ◄───── │ en Render     │
└──────────────────────┘        └────────────────────┘        └───────────────┘
                                         ▲
                                         │ internet
                                ┌────────┴─────────┐
                                │  PANEL WEB       │
                                │  (celu de mamá)  │
                                └──────────────────┘
```

- **Chaca** muestra todo y le cuenta al servidor lo que pasa ("comió", "dijo que sí al almuerzo").
- **El servidor** es un programa que vive en internet (en Render). Recibe lo que manda Chaca, lo guarda y contesta preguntas ("¿cómo están los stats?").
- **La base de datos** guarda todo de forma permanente, como un Excel con tablas.
- **El panel** es una página web con contraseña donde la acompañante ve el resumen.

---

## 2. El hardware (lo físico)

| Pieza | Qué es | Para qué |
|---|---|---|
| **ESP32** (NodeMCU, 38 pines) | El "cerebro": una placa con procesador de 2 núcleos, WiFi y Bluetooth | Corre el programa de Chaca y se conecta a internet |
| **Pantalla TFT 2.4" ILI9341** | Pantalla a color de 240×320 píxeles | Muestra a Chaca |
| **Buzzer pasivo** | Un parlantito que suena según la frecuencia que le mandan | Melodías y sonidos |
| **3 botones** | Nav, Select y Atrás | Moverse, elegir, volver |
| **Portapilas 4×AA** | 4 pilas de 1,5 V = 6 V | Que Chaca funcione sin cable |

### ¿Por qué ESP32 y no Arduino UNO?
- **Tiene WiFi incorporado.** El Arduino UNO no tiene, habría que agregarle un módulo.
- **Tiene más memoria**: los dibujos de Chaca no entrarían en un UNO.
- **Tiene 2 núcleos**: uno dibuja y lee los botones, y el otro habla con internet. Así la pantalla no se traba mientras espera al servidor.

### Las conexiones

| Qué | Pin de la ESP32 |
|---|---|
| Pantalla CS | G5 |
| Pantalla RESET | G22 |
| Pantalla DC | G21 |
| Pantalla MOSI (SDI) | G23 |
| Pantalla SCK | G18 |
| Pantalla MISO (SDO) | G19 |
| Pantalla VCC y LED | 3V3 |
| Buzzer (S) | G26 |
| Botón Nav | G32 |
| Botón Select | G33 |
| Botón Atrás | G25 |
| Todos los GND | GND |
| Pilas (+) | **VIN** (¡nunca 3V3!) |

- **3V3** es una **salida** de 3,3 V que da la placa para la pantalla y el buzzer.
- **VIN** es una **entrada**: ahí se le da corriente a la placa (de 5 a 12 V). Las pilas van ahí.
- **SPI** es el "idioma" con el que la ESP32 le manda el dibujo a la pantalla: SCK es el reloj (el ritmo), MOSI los datos, CS dice "te hablo a vos" y DC dice si lo que va es una orden o un dibujo.
- La velocidad del SPI está en **20 MHz**. Con 40 MHz salían rayas por los cables largos (`TFT_VELOCIDAD`).

### ¿Por qué esos pines y no otros?
En el simulador usábamos una **ESP32-S3**, pero compré una **ESP32 común**. En la común, los pines **6 al 11** los usa la placa por dentro para leer su memoria, así que hubo que mover todo a pines libres. **Los botones van sin resistencias** porque uso `INPUT_PULLUP`: la placa tiene una resistencia interna que deja el pin en "1" y, al apretar, el botón lo conecta a GND y pasa a "0".

---

## 3. El programa de Chaca (`Tamagotchi-Wokwi/sketch.ino`)

Está escrito en **C++ para Arduino**. Es un solo archivo largo, ordenado en secciones numeradas:

| Sección | Qué tiene |
|---|---|
| 1. Librerías | Las herramientas de afuera: WiFi, pantalla, JSON |
| 2. Configuración | WiFi, dirección del servidor, pines |
| 3. Variables globales | Stats, agenda, en qué pantalla estamos |
| 4. Sonido y reloj | Melodías, música del juego, hora de internet |
| 5. Botones | `sePresiono()` |
| 6. Dibujo | Colores, sprites, fondos comprimidos |
| 7. Backend | Hablar con el servidor |
| 8. Pantallas | Una parte por pantalla: Principal, Menú, Comida, Cocina, Dormir, Info, Juego, Agenda, Bienvenida, Baño |
| 9. `irA()` | Cambiar de pantalla |
| 10. `setup()` y `loop()` | El arranque y la vuelta infinita |

### Las 2 funciones que todo Arduino tiene
- **`setup()`** corre **una sola vez** al prender: prepara la pantalla y los botones, muestra la bienvenida y lanza la tarea de red.
- **`loop()`** se repite **para siempre**, unas 20 veces por segundo: lee los botones y, según la pantalla en la que está, decide qué hacer.

### Chaca es una "máquina de estados"
Siempre está en **una sola pantalla** (`pantallaActual`): Bienvenida, Principal, Menú, Comida, Cocina, Baño, Dormir, Juego, Info, Aviso... Para cambiar se usa **`irA(P_...)`**, que dibuja la pantalla nueva. En el `loop()` hay un `switch` que dice qué hacer en cada pantalla cuando apretás un botón.

```
Bienvenida ──Entrar──► Principal ──Select──► Menú ──► Comida ──► Cocina ──► Principal (festeja)
                           ▲                   ├──► Baño ──────────────────► Principal
                           │                   ├──► Dormir ──botón──► Principal (saluda)
                           │                   ├──► Juegos ──► Cabezazos / Atrapa corazones ──Atrás──► Fin del juego ──► Principal
                           │                   └──► Info ──► Respuesta
                           └──── Aviso de la agenda (aparece solo en su horario)
```

### Los 2 núcleos: dibujar y hablar con internet a la vez
- **Núcleo 1**: el `loop()`. Dibuja y lee los botones.
- **Núcleo 0**: la **tarea de red** (`tareaRed`). Se conecta al WiFi, pide la hora, baja stats, preguntas y la agenda, y manda lo que pasa.

¿Por qué separado? Porque el servidor puede tardar varios segundos en contestar (en el plan gratis de Render "se duerme"). Si lo hiciera el `loop()`, Chaca quedaría congelado.

Se comunican con una **cola** (`colaAcciones`): cuando Chaca come, el `loop()` deja un "papelito" en la cola ("alimentar") y sigue con lo suyo. La tarea de red lo agarra y lo manda cuando puede. Si no hay pedidos, cada **1 minuto** pide los stats, y cada **1 hora** vuelve a bajar la agenda, por si mi mamá cambió algún horario.

### Los botones: `sePresiono()`
Cuenta **solo el momento en que se aprieta** (el "flanco"), no todo el tiempo que está apretado. Si no, una sola apretada contaría como 20, una por cada vuelta del `loop()`. En el **juego** es al revés: se lee si está apretado, porque mientras mantenés, Chaca sigue corriendo.

### Los dos juegos
Desde **Menú → Jugar** se abre la lista **JUEGOS** (mismo fondo que las preguntas). En los dos, NAV = izquierda, SELECT = derecha, ATRÁS = terminar, y **nunca se pierde**: la idea es jugar tranquilo, sin sentir que fallaste.
- **Atrapa corazones**: caen corazones (+1) y bombas (−1, nunca baja de 0).
- **Cabezazos**: la pelota cae con "gravedad" (cada cuadro cae un poco más rápido). Si toca la cabeza de Chaca, rebota para arriba: pegarle con el centro la manda derecha, con un costado la manda torcida. Cuando viene justo arriba, Chaca se agacha, y al pegarle se estira. Si toca el piso sale "¡Uy!", la racha vuelve a 0 y cae otra. Cada 5 cabezazos seguidos va un poquito más rápido. Lo que se ajusta está arriba de todo de esa parte del código: `CAB_ALTURA` (qué tan alto sube), `CAB_GRAVEDAD` (qué tan rápido cae), `CAB_AUMENTO` (cuánto más difícil se pone).
- Al terminar, los puntos (corazones, o la mejor racha de cabezazos) suben la felicidad.

### Los stats: hambre, sueño y dopamina
- Van de **0 a 100**.
- **Los calcula el servidor**, no la placa. Chaca los pide cada 1 minuto.
- Si alguno baja de **30** (`ESTADO_BAJO`), Chaca muestra la cara del más bajo: con hambre, bostezando o aburrido.
- Cuando comés o jugás, la placa **sube el número en la pantalla al instante**, con la misma cuenta que el servidor, para no esperar la respuesta.

### La hora
La ESP32 no tiene reloj con pila: al prender no sabe qué hora es. Se la pregunta a un **servidor de hora de internet (NTP)**, y desde ahí la lleva sola. Usa la hora de Argentina (UTC−3). Sin internet no sabe la hora, y por eso la agenda no pregunta y el silencio nocturno no se activa.

### El WiFi
- El nombre y la contraseña de mi WiFi van en **`secretos.h`**, que **no se sube a GitHub** (está en `.gitignore`).
- Si Chaca ve la red **"Wokwi-GUEST"** (la del simulador), usa esa. Si no, busca entre las **redes guardadas desde el celular** (ver abajo), y si no ve ninguna usa la de `secretos.h`. Así el mismo código anda en Wokwi y en la placa real.
- **Configurar el WiFi desde el celular, sin tocar código.** Al prender Chaca con **BACK** apretado, Chaca arma su propia red WiFi llamada **"Chaca"**, solo por un ratito. Te conectás desde el celu y se abre sola una página (como el WiFi de un bar: se llama **portal cautivo**). Elegís la red, ponés la contraseña y Guardar. Chaca la guarda, apaga su red y se reinicia conectándose a la red elegida.
- **Recuerda hasta 5 redes** (casa, escuela...). Quedan en la memoria interna del ESP32 (`Preferences`), que no se borra al apagar. Cada vez que prende, mira qué redes hay cerca y usa la que conoce.
- Por dentro: el ESP32 puede ser **router** además de conectarse a una red. Tiene un mini **servidor web** (`WebServer`) para la página y un **"DNS trampa"** (`DNSServer`) que contesta cualquier dirección con la IP de Chaca (192.168.4.1): por eso el celular abre la página solo.
- **Solo redes de 2.4 GHz.** La ESP32 no se conecta a redes de 5 GHz.

---

## 4. Cómo se dibuja

### Colores RGB565
La pantalla usa colores de **16 bits**: 5 para el rojo, 6 para el verde y 5 para el azul. Por eso los colores en el código son números como `0xFDD7`. `tft.color565(r, g, b)` convierte un color normal (como el de Piskel) a ese formato.

### Los sprites (Chaca, la comida, los íconos)
- Los dibujé en **Piskel**, de **24×24 píxeles**.
- En el código, cada sprite es una **grilla de letras**: cada letra es un color de la **paleta** y el `.` es transparente.
  ```
  "....AAAA...."
  "..AABBBBAA.."     A = contorno gris oscuro, B = piel, etc.
  ```
- Se dibujan **agrandados** (escala 5: cada píxel del dibujo es un cuadrado de 5×5), así 24×24 ocupa 120×120 en la pantalla.
- El `.` no se pinta, por eso se ve el fondo detrás: **transparencia**.
- Están en la carpeta `sprites/` (`sprites.h`, `comida.h`, `iconos.h`...).

### Los fondos (living, cocina, menú, parque, baño...)
- Son de **240×320**, la pantalla entera.
- Están **comprimidos con RLE**: en vez de guardar "celeste, celeste, celeste, celeste", se guarda **"celeste ×4"**. Un fondo de colores planos ocupa mucho menos.
- Están en la carpeta `fondos/` y viven en la memoria **flash**, la grande, no en la RAM.
- `dibujarFondoRLE()` los descomprime mientras dibuja, de izquierda a derecha y de arriba hacia abajo, como se lee un libro.

### Redibujo por zonas (para que no parpadee)
Redibujar la pantalla entera es lento y parpadea. Entonces, cuando Chaca se mueve, se redibuja **solo el rectángulo donde está Chaca**:
1. `empezarZona()` arma un **lienzo en memoria** (`GFXcanvas16`), una "pantalla de mentira" en la RAM, con el pedacito de fondo de esa zona.
2. Se dibuja Chaca encima, en el lienzo.
3. `terminarZona()` manda el lienzo **de una sola vez** a la pantalla.

Así no se ve el fondo "borrándose" y Chaca encima: aparece todo junto.

### Las animaciones
Una animación es una **lista de dibujos** (cuadros) que se van cambiando según el tiempo. Se usa **`millis()`**, los milisegundos desde que prendió la placa. Por ejemplo, el saludo cambia de dibujo cada 300 ms:
```cpp
SPR_SALUDO[(t / 300) % N_SALUDO]
```
`t / 300` dice cuántos "turnos de 300 ms" pasaron, y `% N_SALUDO` vuelve al primero cuando se terminan los dibujos. **No se usa `delay()`** para animar, porque congelaría todo, botones incluidos.

### De Piskel al código (carpeta `herramientas/`)
- **Sprites**: `python herramientas/png_a_sprite.py imagenes/dibujo.png` → imprime la grilla de letras y se pega en el `.h` que corresponda.
- **Fondos**: `python herramientas/png_a_rle.py imagenes/fondo.png fondos/fondo.h FONDO_NOMBRE` → genera el `.h` comprimido.
- Consejo: colores planos, sin degradés, así el fondo comprime mejor.

---

## 5. El sonido

- El **buzzer pasivo** suena a la frecuencia que le mandan: 440 Hz es un La, 523 un Do...
- Cada melodía es una lista de **notas** `{frecuencia, duración}`. Frecuencia 0 es un silencio.
- `tone()` tiene su propia **cola**: se mandan todas las notas juntas y suenan solas, una detrás de otra, sin trabar la pantalla.
- **La música del juego** es distinta: se manda **nota por nota** desde el `loop()` (`seguirMusica()`). Si la mandara entera, el sonido de agarrar un corazón quedaría esperando a que termine la canción. Es una melodía inventada para Chaca, en Do mayor (por eso suena alegre). `MUS_CORCHEA` controla la velocidad: más chico, más rápido.
- **Silencio nocturno**: de **22 a 8** solo suena el clic de los botones. El horario se cambia desde el panel.

---

## 6. La agenda: cómo pregunta Chaca

La rutina está en **`rutina.json`**, en el servidor. Cada **rango** es un horario con su pregunta:

| Rango | Horario | Pregunta |
|---|---|---|
| Desayuno | 08:00 a 11:00 | ¿Ya desayunaste? |
| Almuerzo | 12:00 a 15:00 | ¿Ya almorzaste? |
| Cena | 19:00 a 22:00 | ¿Ya cenaste? |
| Higiene | 08:00 a 21:00 | ¿Te lavaste cara y dientes? |
| Actividad del día | 16:00 a 19:00 | Una por día, rotando: caminar, abrir la ventana, música, hacer la cama, dibujar |
| Dormir | 21:00 a 01:00 | ¿Vamos a dormir? |

### Las reglas
- Chaca pregunta **solo si está tranquilo** en la pantalla principal, nunca en medio del juego.
- **Tres respuestas**: **Sí** / **Más tarde** / **Hoy paso**.
- **Sí**: Chaca lo hace con vos. Si es comida, eligen la comida y va a la cocina. Si es baño, se baña. Si es dormir, se duerme. Si es la actividad, festeja.
- **Más tarde**: vuelve a preguntar **a los 45 minutos**, como mucho **3 veces** por rango.
- **Micro-tareas**: después de un "Más tarde", propone algo **más chico** ("¿Picaste algo?", "¿Te lavaste las manos?"). Si decís que sí, **cuenta igual que la tarea completa** y Chaca festeja igual. (Decisión de mi mamá: lo importante es hacer algo.)
- **Hoy paso**: es una respuesta válida, **no un fracaso**. Chaca saluda y no vuelve a preguntar por eso hasta mañana.
- La **actividad del día** pregunta **una sola vez**, sin insistir.
- Si nadie contesta en **1 minuto**, Chaca vuelve a lo suyo.
- Entre una pregunta y otra pasan **al menos 2 minutos**: nunca dos seguidas.
- Si termina el horario sin ninguna respuesta, se anota **"sin respuesta" en silencio**: solo lo ve la acompañante, a la persona no se le reprocha nada.
- Si la placa se reinicia, le pregunta al servidor qué ya se respondió hoy, así **no vuelve a preguntar lo mismo**.

### Por qué está pensado así
Para que Chaca **acompañe y no presione**: sin reproches, sin insistir de más, sin hacer sentir mal a la persona si un día no puede.

---

## 7. El servidor (`server.js`)

Es un programa en **Node.js** (JavaScript fuera del navegador) con **Express**, una librería para armar servidores web. Vive en **Render**.

### Qué es un "endpoint"
Es una **dirección** a la que Chaca le pide o le manda algo. Hay dos tipos:
- **GET**: traer algo ("dame los stats").
- **POST**: mandar algo ("Chaca comió").

Las respuestas vienen en **JSON**, un formato de texto con datos:
```json
{"nombre": "Chaca", "hambre": 80, "sueno": 65, "dopamina": 90}
```

### Los endpoints de Chaca

| Endpoint | Qué hace |
|---|---|
| `GET /estado` | Devuelve los stats actuales (y anota que Chaca se conectó) |
| `POST /accion` | Chaca comió / jugó / se bañó / se durmió / se despertó |
| `GET /preguntas` | Las preguntas de Info con sus respuestas |
| `GET /rutina` | La agenda del día y qué ya se respondió |
| `POST /registro` | Lo que respondió la persona: sí, más tarde, hoy paso, sin respuesta |
| `GET /health` | Para que Render sepa si el servidor está vivo |

Las del panel empiezan con `/panel` y están en **`panel.js`**.

### Cómo bajan los stats (lo más ingenioso)
**No se guarda "el hambre bajando en vivo".** Se guarda **el último valor y cuándo se guardó**. Cuando Chaca pregunta, el servidor calcula cuánto bajó desde entonces (`calcularEstadoActual`):
- **Hambre**: baja 0,2 por minuto → de 100 a 0 en unas **8 horas**.
- **Sueño**: baja 0,1 por minuto → en unas **16 horas**. Mientras Chaca duerme, **sube** 0,3 por minuto: se recupera en unas 6 horas.
- **Dopamina**: baja 0,2 por minuto.
- **Comer**: +30 de hambre. **Jugar**: +5, más 3 por corazón, hasta 50.

Así, aunque Chaca esté apagada, cuando vuelve los stats bajaron lo que tenían que bajar.

### Las preguntas de Info
Se escriben en **`preguntas.json`**. Cada vez que el servidor arranca, las copia a la base. Chaca las baja al prender, **con las respuestas**, así contesta al instante. Si no hay internet, usa una copia que está dentro del código.

---

## 8. La base de datos (PostgreSQL)

Es como un **Excel con varias hojas (tablas)**, pero que vive en internet y no se pierde.

| Tabla | Qué guarda |
|---|---|
| `estado_mascota` | Los stats de Chaca y cuándo se conectó por última vez |
| `historial_acciones` | Todo lo que pasó, con fecha y hora: comió, confirmó el almuerzo, pospuso, hoy paso, sin respuesta... |
| `seguimiento_avisos` | Cuántas veces preguntó Chaca por cada rango, cada día |
| `configuracion` | Los horarios que la acompañante cambió desde el panel |
| `notas_panel` | Las notas que escribe la acompañante |
| `respuestas_fijas` | Las preguntas y respuestas de Info |

- Todas están descritas en **`schema.sql`**. El servidor lo ejecuta solo cada vez que arranca: con una base nueva y vacía, crea todo.
- **¿Por qué los horarios del panel van a la base y no a `rutina.json`?** Porque el disco de Render **se borra** cada vez que se reinicia el servidor. La base no.

---

## 9. El panel del acompañante

- **Dirección:** https://tamagotchi-api-9vk6.onrender.com/panel
- **Con datos de ejemplo:** https://tamagotchi-api-9vk6.onrender.com/panel?demo=1

- **Entrar**: usuario y contraseña (`PANEL_USUARIO` y `PANEL_CLAVE`, configurados en Render). Queda la sesión abierta 30 días.
- **Seguimiento**: una grilla de 14 o 30 días con cada rango (confirmó, confirmó después de posponer, hoy paso, sin respuesta), el detalle de hoy y el historial.
- **Semáforo** (lo decidió mi mamá):
  - 🟢 **Normal**: todas las áreas con alguna respuesta en los últimos 2 días.
  - 🟡 **Atención moderada**: un área (comida, higiene, actividad o descanso) sin ninguna respuesta hace 2 días o más.
  - 🔴 **Atención**: ninguna respuesta en nada hace más de 3 días.
  - "Responder" cuenta cualquier respuesta, incluso "Más tarde" u "Hoy paso": lo que importa es que la persona esté.
  - **"Sin conexión"** aparece aparte, si Chaca no habló con el servidor hace más de 30 minutos.
- **Rutina**: cambiar los horarios. Chaca los toma en menos de una hora.
- **Notas**: para que la acompañante anote lo que quiera.
- **Privacidad**: solo lo ven la persona y su acompañante, con contraseña.

---

## 10. Render y GitHub

- **GitHub** (github.com/amparo242008-afk/tamagotchi-backend) guarda todo el código y cada cambio, con fecha y autora.
- **Render** está conectado a GitHub: **cada vez que subo un cambio, Render actualiza el servidor solo** en uno o dos minutos.
- **Plan gratis de Render**, lo que hay que saber:
  - El servidor **se duerme** si nadie le habla en 15 minutos, y tarda alrededor de 1 minuto en despertar. Por eso Chaca espera hasta 60 segundos y reintenta 3 veces.
  - **750 horas gratis por mes**, compartidas entre todos los servicios de la cuenta. Chaca prendida todo el mes usa unas 720.
  - **La base de datos gratis vence a los 30 días** de crearla. La de la prueba se crea el 29/10 y dura hasta el ~28/11, después de la expo (que es a más tardar el 27/11).

---

## 11. Cómo hago... (recetas)

### Subir el código a la placa
1. Enchufá la placa a la compu, **en un puerto de atrás**.
2. En VS Code, abajo, tocá la **flechita →** de PlatformIO (*Upload*).
3. Cuando en la terminal aparezca **`Connecting....`**, **mantené apretado BOOT** hasta que empiece a decir `Writing at...`. Ahí lo soltás.
4. Cuando dice `SUCCESS`, listo: la placa se reinicia sola.

*(Mi placa no entra sola en modo de carga, por eso hace falta BOOT.)*

### Ver qué dice la placa (monitor serie)
Abajo en VS Code, el **enchufe** de PlatformIO (*Serial Monitor*). Muestra cosas como "WiFi conectado" u "Hora: 18:30". Sirve para buscar errores.

### Cambiar el WiFi
1. Desenchufá Chaca. **Mantené apretado BACK** y enchufala. Aparece "Configurar WiFi".
2. En el celu, conectate a la red **Chaca** (no tiene contraseña). Se abre una página; si no, entrá a **192.168.4.1** en el navegador.
3. Elegí la red, escribí la contraseña y tocá **Guardar**. Chaca se reinicia y se conecta.
- Para salir sin cambiar nada: apretá **BACK**.
- La de `secretos.h` sigue siendo la red "de fábrica" (cambiarla sí necesita subir el código). ¡No toques `secretos.ejemplo.h`!
- En Wokwi esto no se puede probar (el celular no llega al simulador).

### Cambiar una respuesta de Info
1. En `preguntas.json` (el servidor), cambiá la respuesta y subí a GitHub.
2. En `sketch.ino`, buscá `respuestasFijas` y cambiala también (es la copia para cuando no hay internet). Subila a la placa.

### Cambiar una pregunta de la agenda o una micro-tarea
En `rutina.json`, cambiá el texto (`"pregunta"` o `"micro"`) y subí a GitHub. Chaca lo toma en menos de una hora, o al reiniciarla.

### Cambiar un horario
Desde el **panel**, pestaña **Rutina**. No hace falta tocar código.

### Activar el modo demo
- En la placa: **mantené apretado SELECT mientras la enchufás**.
- En Wokwi: en `sketch.ino` poné `#define MODO_DEMO_SIEMPRE true`, compilá y arrancá. **Volvelo a `false` después.**
- En modo demo Chaca recorre todo sola, no usa internet y no ensucia el historial. Un botón de verdad la corta.

### Probar en el simulador (Wokwi)
Compilá con el **✓** y después **F1 → "Wokwi: Start Simulator"**.

---

## 12. Si algo falla

| Problema | Qué hacer |
|---|---|
| La compu no reconoce la placa | Probá otro cable USB (algunos solo cargan). Fijate que esté el driver **CP210x** instalado |
| Al subir dice `Wrong boot mode` o se queda en `Connecting....` | Mantené **BOOT** apretado mientras dice `Connecting` |
| La pantalla queda negra | ¿El cable **LED** está en 3V3? ¿VCC en 3V3? |
| La pantalla tiene rayas o manchas | Bajá `TFT_VELOCIDAD` a `10000000` en `sketch.ino`. Revisá los cables SCK y MOSI |
| No suena | ¿Son entre las 22 y las 8? Es el silencio nocturno. Si no, revisá que el buzzer esté en **G26** |
| Chaca entra a la casa sin tocar nada | Hay algo conectado en un pin de botón (G32, G33 o G25) que no debería |
| No se conecta al WiFi | ¿La red es de **2.4 GHz**? Volvé a cargarla prendiendo con **BACK** (quizá la contraseña estaba mal) |
| Prendí con BACK y el celu no abre la página | Entrá a mano a **192.168.4.1**. Si el celu dice "sin internet, ¿desconectar?", decile que **no** / seguir conectado |
| Tarda mucho en mostrar los stats | El servidor de Render se estaba despertando. Esperá un minuto |
| El panel dice "Sin conexión" | Chaca está apagada o sin internet |
| Se queda en la pantalla de inicio, se reinicia sola y el buzzer zumba | Le falta energía: en el monitor serie dice **"Brownout detector was triggered"**. Enchufala en un **puerto USB de atrás** de la compu o en un cargador de celular (los de adelante dan menos corriente) |
| Se reinicia sola con pilas o batería | Pilas gastadas o batería baja: cambialas o cargala |
| En la presentación no hay WiFi | **Usá el modo demo**: no necesita internet |

---

## 13. Preguntas que me puede hacer el jurado

**¿Por qué ESP32 y no Arduino UNO?**
Porque tiene WiFi incorporado, mucha más memoria para los dibujos y dos núcleos: uno dibuja y el otro habla con internet, así nunca se traba.

**¿Qué pasa si no hay internet?**
Chaca sigue funcionando como mascota: se puede alimentar, bañar, jugar y contesta las preguntas de Info, porque las tiene guardadas. Lo que no hace es la agenda (no sabe la hora) ni mandarle datos al panel. Cuando vuelve internet, se pone al día sola.

**¿Dónde se guardan los datos? ¿Son privados?**
En una base de datos PostgreSQL en Render. El panel tiene usuario y contraseña: solo lo ven la persona y su acompañante.

**¿Cómo bajan los stats si Chaca está apagada?**
El servidor guarda el último valor y la hora. Cuando Chaca pregunta, calcula cuánto bajó por el tiempo que pasó.

**¿Por qué "Hoy paso"?**
Porque no hacer algo un día no es un fracaso. Queríamos que Chaca acompañe sin presionar ni hacer sentir culpa.

**¿Por qué no insiste más veces?**
Insistir demasiado se vuelve molesto y la persona termina apagándolo. Como mucho 3 veces, cada 45 minutos, y después de la primera propone algo más chico.

**¿Chaca reemplaza la terapia?**
No. Es un acompañante para la rutina diaria. No diagnostica ni trata nada. Sirve como apoyo y como una forma de que alguien de confianza sepa cómo venís, sin estar encima.

**¿Hiciste vos los dibujos?**
Sí, en Piskel, píxel por píxel. Después los convertí a código con unas herramientas en Python.

**¿Cómo se ve a Chaca moverse sin que parpadee la pantalla?**
Se redibuja solo la zona donde está Chaca, armándola primero en la memoria y mandándola de una sola vez.

**¿Qué fue lo más difícil?**
Pasar del simulador a la placa real: instalar el driver, cambiar los pines porque la placa era distinta, y las rayas en la pantalla por la velocidad. También que el servidor gratis se dormía y Chaca tenía que esperarlo.

**¿Por qué el servidor y la base están en internet y no en la placa?**
Para que la acompañante pueda ver el panel desde cualquier lado, y para que los datos no se pierdan si la placa se apaga o se rompe.

**¿Qué mejorarías?**
Una carcasa final, una batería recargable, más actividades y preguntas personalizables, y mostrárselo a profesionales de la salud para mejorarlo.

---

## 14. Glosario

| Palabra | Qué es |
|---|---|
| **Firmware** | El programa que corre dentro de la placa (`sketch.ino`) |
| **Sprite** | Un dibujo chiquito que se mueve (Chaca, la comida) |
| **RLE** | Forma de comprimir: "celeste ×4" en vez de "celeste, celeste, celeste, celeste" |
| **RGB565** | Colores de 16 bits que usa la pantalla |
| **SPI** | El "idioma" para mandarle el dibujo a la pantalla |
| **GPIO** | Un pin de la placa que se puede usar como entrada o salida (G26, G32...) |
| **INPUT_PULLUP** | Modo de pin con una resistencia interna: suelto es 1, apretado es 0 |
| **millis()** | Milisegundos desde que prendió la placa. Se usa para animar sin congelar |
| **Máquina de estados** | Estar siempre en un estado (pantalla) y pasar de uno a otro |
| **Núcleo** | Cada "cerebro" del procesador. El ESP32 tiene 2 |
| **Cola** | Lista de pedidos pendientes entre el loop y la tarea de red |
| **NTP** | Servidor de internet que da la hora |
| **Servidor / backend** | El programa que vive en internet y guarda los datos |
| **Endpoint** | Una dirección del servidor (`/estado`, `/accion`...) |
| **GET / POST** | Pedir algo / mandar algo |
| **JSON** | Formato de texto para mandar datos |
| **Base de datos** | Donde se guardan los datos para siempre, en tablas |
| **Render** | Donde viven el servidor y la base, en internet |
| **GitHub** | Donde se guarda el código y cada cambio |
| **Deploy** | Publicar una versión nueva del servidor |
| **Wokwi** | El simulador donde probaba a Chaca antes de tener la placa |
| **PlatformIO** | La extensión de VS Code para compilar y subir el código |
| **Piskel** | El programa donde dibujé los sprites |
