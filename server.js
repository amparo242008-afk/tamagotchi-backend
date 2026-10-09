// server.js - Backend del Tamagotchi
// Node.js + Express + PostgreSQL
//
// Qué es esto: un programa que corre en Render (en internet) y le contesta a Roberto.
// Roberto le pide cosas a una dirección (ej. GET /estado) y el servidor contesta en JSON.
// Los datos quedan guardados en una base de datos Postgres (tablas, como en Excel).
//
// Índice:
//   1. Conexión a Postgres
//   2. Configuración (qué tan rápido bajan los stats)
//   3. calcularEstadoActual: cuánto bajó cada stat desde la última vez
//   4. GET  /estado          -> los stats de Roberto ahora
//   5. POST /accion          -> Roberto comió / jugó / se durmió / se despertó
//   6. GET  /respuesta/:clave y GET /preguntas -> las preguntas de Info
//   6c. GET /rutina y POST /registro -> la agenda (avisos de comer, bañarse, dormir)
//   7. prepararBase y arranque del servidor
//
// Cómo se lee un "endpoint":  app.get('/estado', async (req, res) => { ... })
//   - app.get / app.post: a qué tipo de pedido contesta (GET = traer, POST = mandar algo)
//   - '/estado': la dirección
//   - req (request): lo que mandó Roberto (req.body = los datos del POST)
//   - res (response): la contestación; res.json({...}) la manda
//   - async / await: esperar a la base de datos sin trabar el servidor
//   - try / catch: si algo falla, se contesta un error 500 en vez de que se caiga todo

const express = require('express');   // la librería para armar el servidor web
const { Pool } = require('pg');       // la librería para hablar con Postgres
require('dotenv').config();           // lee el archivo .env (contraseñas locales)

const app = express();
app.use(express.json());              // para entender los pedidos que vienen en JSON

// Render inyecta PORT automáticamente; en local caemos a 3000.
const PORT = process.env.PORT || 3000;

// ---------------------------------------------
// 1. CONEXIÓN A POSTGRES
// ---------------------------------------------
// En Render (y en producción) la conexión viene por la variable de
// entorno DATABASE_URL, que Render genera automáticamente si creás
// una base de datos Postgres desde el panel.
// Localmente podés usar un archivo .env (ver .env.example).
const connectionString = process.env.DATABASE_URL;

const pool = new Pool(
    connectionString
        ? { connectionString, ssl: { rejectUnauthorized: false } }
        : {
              user: process.env.PGUSER || 'postgres',
              host: process.env.PGHOST || 'localhost',
              database: process.env.PGDATABASE || 'tamagotchi',
              password: process.env.PGPASSWORD || '',
              port: Number(process.env.PGPORT) || 5432,
          }
);

// ---------------------------------------------
// 2. CONFIGURACIÓN DE DECAIMIENTO
// ---------------------------------------------
// MODO_DEMO = true: todo pasa rápido, para mostrar en la defensa o para probar.
// MODO_DEMO = false: ritmo tranquilo de uso diario, así Roberto no está siempre triste.
const MODO_DEMO = false;

// Cuántos puntos baja cada stat por minuto que pasa
const DECAIMIENTO_POR_MINUTO = MODO_DEMO
    ? { hambre: 1, sueno: 0.5, dopamina: 1.5 }       // hambre de 100 a 0 en ~1,5 h
    : { hambre: 0.2, sueno: 0.1, dopamina: 0.2 };    // hambre de 100 a 0 en ~8 h, sueño en ~16 h

// Mientras Roberto duerme, el sueño SUBE (en vez de bajar) a este ritmo.
// Demo: de 0 a 100 en 10 minutos. Real: en unas 6 horas (una noche).
const RECUPERACION_SUENO_POR_MINUTO = MODO_DEMO ? 10 : 0.3;

// Minijuego "Atrapar": dopamina = base por jugar + puntos por corazón, con un tope por partida
const JUGAR_BASE = 5;
const JUGAR_POR_CORAZON = 3;
const JUGAR_MAXIMO = 50;

// ---------------------------------------------
// 3. LA FUNCIÓN CLAVE: calcular el estado actual
// ---------------------------------------------
// No guardamos "hambre bajando en tiempo real". Guardamos el último
// valor conocido + cuándo se guardó, y acá calculamos cuánto bajó
// desde entonces.
function calcularEstadoActual(registro) {
    const ahora = new Date();
    const ultimaVez = new Date(registro.ultima_actualizacion);
    const minutosPasados = (ahora - ultimaVez) / 1000 / 60;

    // Restamos el decaimiento correspondiente a cada stat,
    // sin dejar que baje de 0
    const hambre = Math.max(0, registro.hambre - minutosPasados * DECAIMIENTO_POR_MINUTO.hambre);
    // Si está durmiendo, el sueño se recupera (hasta 100) en vez de bajar
    const sueno = registro.durmiendo
        ? Math.min(100, Number(registro.sueno) + minutosPasados * RECUPERACION_SUENO_POR_MINUTO)
        : Math.max(0, registro.sueno - minutosPasados * DECAIMIENTO_POR_MINUTO.sueno);
    const dopamina = Math.max(0, registro.dopamina - minutosPasados * DECAIMIENTO_POR_MINUTO.dopamina);

    return {
        nombre: registro.nombre,
        hambre: Math.round(hambre),
        sueno: Math.round(sueno),
        dopamina: Math.round(dopamina),
        durmiendo: Boolean(registro.durmiendo),
    };
}

// ---------------------------------------------
// 3b. RUTA RAÍZ Y HEALTH CHECK
// ---------------------------------------------
// Render usa /health para saber si el servicio está vivo.
app.get('/', (req, res) => {
    res.json({ nombre: 'Tamagotchi API', estado: 'ok' });
});

app.get('/health', async (req, res) => {
    try {
        await pool.query('SELECT 1');
        res.json({ status: 'ok', db: 'conectada' });
    } catch (err) {
        console.error(err);
        res.status(500).json({ status: 'error', db: 'sin conexion' });
    }
});

// ---------------------------------------------
// 4. ENDPOINT: GET /estado
// ---------------------------------------------
// Devuelve el estado actual de la mascota (ya con el decaimiento aplicado)
app.get('/estado', async (req, res) => {
    try {
        const result = await pool.query('SELECT * FROM estado_mascota WHERE id = 1');
        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'No existe la mascota todavía' });
        }
        const estadoActual = calcularEstadoActual(result.rows[0]);
        res.json(estadoActual);
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: 'Error al consultar el estado' });
    }
});

// ---------------------------------------------
// 5. ENDPOINT: POST /accion
// ---------------------------------------------
// Body esperado: { "tipo": "alimentar" | "jugar" | "dormir" | "despertar" }
// - alimentar / jugar: suben su stat de una.
// - dormir: Roberto se acuesta; el sueño va subiendo con el tiempo (ver calcularEstadoActual).
// - despertar: Roberto se levanta; el sueño vuelve a bajar normal.
app.post('/accion', async (req, res) => {
    const { tipo } = req.body;

    // Mapeo de acción -> qué columna sube y cuánto
    const acciones = {
        alimentar: { columna: 'hambre', suma: 30 },
        jugar: { columna: 'dopamina', suma: 30 },
        dormir: { durmiendo: true },
        despertar: { durmiendo: false },
    };

    const accion = acciones[tipo];
    if (!accion) {
        return res.status(400).json({ error: 'Tipo de acción inválido' });
    }

    try {
        // Primero traemos el estado actual (ya con decaimiento aplicado)
        // para no sumarle a un valor viejo
        const result = await pool.query('SELECT * FROM estado_mascota WHERE id = 1');
        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'No existe la mascota todavia' });
        }
        const estadoActual = calcularEstadoActual(result.rows[0]);

        // No se puede dormir si no tiene sueño
        if (tipo === 'dormir' && estadoActual.sueno >= 100) {
            return res.status(409).json({ error: 'No tiene sueño', ...estadoActual });
        }

        const nuevoEstado = {
            hambre: estadoActual.hambre,
            sueno: estadoActual.sueno,
            dopamina: estadoActual.dopamina,
            durmiendo: estadoActual.durmiendo,
        };
        if (tipo === 'jugar') {
            // Minijuego: haber jugado ya suma algo, y cada corazón atrapado suma más
            const puntos = Math.max(0, Math.floor(Number(req.body.puntos) || 0));
            const suma = Math.min(JUGAR_MAXIMO, JUGAR_BASE + puntos * JUGAR_POR_CORAZON);
            nuevoEstado.dopamina = Math.min(100, nuevoEstado.dopamina + suma);
        } else if (accion.columna) {
            nuevoEstado[accion.columna] = Math.min(100, nuevoEstado[accion.columna] + accion.suma);
        }
        if (accion.durmiendo !== undefined) {
            nuevoEstado.durmiendo = accion.durmiendo;
        }

        // Se guardan los TRES stats ya recalculados (si no, los otros "reviven")
        await pool.query(
            `UPDATE estado_mascota
             SET hambre = $1, sueno = $2, dopamina = $3, durmiendo = $4, ultima_actualizacion = NOW()
             WHERE id = 1`,
            [nuevoEstado.hambre, nuevoEstado.sueno, nuevoEstado.dopamina, nuevoEstado.durmiendo]
        );

        // Historial para el futuro panel web del acompañante
        await pool.query('INSERT INTO historial_acciones (tipo) VALUES ($1)', [tipo]);

        res.json({ mensaje: `${tipo} aplicado`, ...nuevoEstado });
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: 'Error al aplicar la acción' });
    }
});

// ---------------------------------------------
// 6. ENDPOINT: GET /respuesta/:clave
// ---------------------------------------------
// Busca en respuestas_fijas por pregunta_clave (ej: "nombre", "juego_favorito")
app.get('/respuesta/:clave', async (req, res) => {
    const { clave } = req.params;
    try {
        const result = await pool.query(
            'SELECT respuesta FROM respuestas_fijas WHERE pregunta_clave = $1',
            [clave]
        );
        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'No tengo respuesta para eso' });
        }
        res.json({ respuesta: result.rows[0].respuesta });
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: 'Error al buscar la respuesta' });
    }
});

// ---------------------------------------------
// 6b. ENDPOINT: GET /preguntas
// ---------------------------------------------
// La lista de preguntas que muestra Roberto en Info, en orden: [{ clave, pregunta }]
app.get('/preguntas', async (req, res) => {
    try {
        const result = await pool.query(
            'SELECT pregunta_clave AS clave, pregunta FROM respuestas_fijas ORDER BY orden'
        );
        res.json(result.rows);
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: 'Error al buscar las preguntas' });
    }
});

// ---------------------------------------------
// 6c. AGENDA DE ROBERTO: GET /rutina y POST /registro
// ---------------------------------------------
// Los rangos horarios se escriben en rutina.json. Roberto pregunta en cada rango
// ("¿ya comiste?") y la persona contesta con un botón. Eso queda en historial_acciones
// para el panel del acompañante. La persona nunca ve lo que no hizo.

// Argentina es UTC-3 todo el año (no hay horario de verano)
const OFFSET_ARGENTINA_MS = -3 * 60 * 60 * 1000;

// "08:30" -> 510 (minutos desde la medianoche)
function aMinutos(hhmm) {
    const [h, m] = hhmm.split(':').map(Number);
    return h * 60 + m;
}

// Cuándo empezó (en hora real) la vez más reciente de este rango.
// Ej: a las 00:30, el rango "dormir" 21:00–01:00 empezó AYER a las 21:00.
function inicioUltimoRango(rango) {
    const ahoraAr = new Date(Date.now() + OFFSET_ARGENTINA_MS);
    const minutosAhora = ahoraAr.getUTCHours() * 60 + ahoraAr.getUTCMinutes();
    const desde = aMinutos(rango.desde);
    const inicioAr = new Date(ahoraAr);
    inicioAr.setUTCHours(Math.floor(desde / 60), desde % 60, 0, 0);
    if (minutosAhora < desde) inicioAr.setUTCDate(inicioAr.getUTCDate() - 1);
    return new Date(inicioAr.getTime() - OFFSET_ARGENTINA_MS);
}

// El día (en Argentina) en que empezó esa vuelta del rango, ej. "2026-10-09".
// Es la fecha de la fila en seguimiento_avisos (el "dormir" de las 00:30 cuenta para ayer).
function fechaDelRango(rango) {
    return new Date(inicioUltimoRango(rango).getTime() + OFFSET_ARGENTINA_MS).toISOString().slice(0, 10);
}

// Lee rutina.json de nuevo cada vez (borrando la copia guardada), así si se edita
// el archivo no hace falta reiniciar el servidor.
function leerRutina() {
    delete require.cache[require.resolve('./rutina.json')];
    return require('./rutina.json');
}

// GET /rutina: Roberto la pide al prender. Le devuelve rutina.json y, para cada rango,
// lo que ya pasó hoy (si ya dijo "sí", cuántas veces preguntó y hace cuánto).
app.get('/rutina', async (req, res) => {
    try {
        const rutina = leerRutina();
        const rangos = [];
        for (const r of rutina.rangos) {
            // ¿ya se contestó "sí" en esta vuelta del rango? (así un reinicio no repregunta)
            const result = await pool.query(
                `SELECT 1 FROM historial_acciones
                 WHERE clave = $1 AND tipo LIKE 'confirmo_%' AND fecha >= $2 LIMIT 1`,
                [r.clave, inicioUltimoRango(r)]
            );
            // cuántas veces ya preguntó hoy, para que un reinicio no vuelva a contar desde 0
            const seguimiento = await pool.query(
                `SELECT cantidad_insistencias,
                        FLOOR(EXTRACT(EPOCH FROM NOW() - ultima_insistencia) / 60) AS min_desde_ultimo
                 FROM seguimiento_avisos WHERE tipo = $1 AND fecha = $2`,
                [r.clave, fechaDelRango(r)]
            );
            const fila = seguimiento.rows[0];
            rangos.push({
                ...r,
                confirmado: result.rows.length > 0,
                avisos: fila ? fila.cantidad_insistencias : 0,
                min_desde_ultimo: fila && fila.min_desde_ultimo !== null ? Number(fila.min_desde_ultimo) : null,
            });
        }
        res.json({
            reinsistir_min: rutina.reinsistir_min,
            max_avisos: rutina.max_avisos,
            silencio: rutina.silencio,
            rangos,
        });
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: 'Error al leer la rutina' });
    }
});

// Body: { "clave": "almuerzo", "respuesta": "aviso" | "si" | "mas_tarde" | "sin_respuesta" }
// "aviso" = Roberto acaba de preguntar (no va al historial, solo suma en seguimiento_avisos).
app.post('/registro', async (req, res) => {
    const { clave, respuesta } = req.body;
    const prefijos = { si: 'confirmo', mas_tarde: 'posterga', sin_respuesta: 'sin_respuesta' };
    const rango = leerRutina().rangos.find((r) => r.clave === clave);
    if (!rango || (!prefijos[respuesta] && respuesta !== 'aviso')) {
        return res.status(400).json({ error: 'Registro inválido' });
    }
    const tipo = `${prefijos[respuesta]}_${rango.actividad}`;
    try {
        // seguimiento_avisos: una fila por rango y por día (la lee el panel del acompañante)
        if (respuesta === 'aviso') {
            await pool.query(
                `INSERT INTO seguimiento_avisos (tipo, fecha, cantidad_insistencias, ultima_insistencia)
                 VALUES ($1, $2, 1, NOW())
                 ON CONFLICT (tipo, fecha) DO UPDATE
                 SET cantidad_insistencias = seguimiento_avisos.cantidad_insistencias + 1,
                     ultima_insistencia = NOW()`,
                [clave, fechaDelRango(rango)]
            );
            return res.json({ mensaje: 'aviso anotado' });
        }
        if (respuesta === 'si') {
            await pool.query(
                `INSERT INTO seguimiento_avisos (tipo, fecha, completado)
                 VALUES ($1, $2, TRUE)
                 ON CONFLICT (tipo, fecha) DO UPDATE SET completado = TRUE`,
                [clave, fechaDelRango(rango)]
            );
        }
        // "sin respuesta" se anota una sola vez por rango (por si la placa se reinicia)
        if (respuesta === 'sin_respuesta') {
            const ya = await pool.query(
                `SELECT 1 FROM historial_acciones
                 WHERE clave = $1 AND (tipo LIKE 'confirmo_%' OR tipo LIKE 'sin_respuesta_%') AND fecha >= $2 LIMIT 1`,
                [clave, inicioUltimoRango(rango)]
            );
            if (ya.rows.length > 0) return res.json({ mensaje: 'ya registrado' });
        }
        await pool.query('INSERT INTO historial_acciones (tipo, clave) VALUES ($1, $2)', [tipo, clave]);
        res.json({ mensaje: 'registrado', tipo });
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: 'Error al registrar' });
    }
});

// ---------------------------------------------
// 7. LEVANTAR EL SERVER
// ---------------------------------------------
// Antes de arrancar, agrega a la base lo que haga falta (si ya existe, no hace nada).
// Así no hay que correr SQL a mano en Render cada vez que se suma algo.
async function prepararBase() {
    await pool.query(
        'ALTER TABLE estado_mascota ADD COLUMN IF NOT EXISTS durmiendo BOOLEAN NOT NULL DEFAULT false'
    );
    await pool.query(
        `CREATE TABLE IF NOT EXISTS historial_acciones (
            id    SERIAL PRIMARY KEY,
            tipo  TEXT        NOT NULL,
            fecha TIMESTAMPTZ NOT NULL DEFAULT NOW()
        )`
    );
    // clave: de que rango de la rutina es el registro (ej. "almuerzo"); vacío en las acciones del juguete
    await pool.query('ALTER TABLE historial_acciones ADD COLUMN IF NOT EXISTS clave TEXT');
    // tipo = clave del rango (desayuno, almuerzo, cena, bano, dormir)
    await pool.query(
        `CREATE TABLE IF NOT EXISTS seguimiento_avisos (
            id                    SERIAL PRIMARY KEY,
            tipo                  TEXT        NOT NULL,
            fecha                 DATE        NOT NULL,
            cantidad_insistencias INTEGER     NOT NULL DEFAULT 0,
            ultima_insistencia    TIMESTAMPTZ,
            completado            BOOLEAN     NOT NULL DEFAULT FALSE,
            UNIQUE (tipo, fecha)
        )`
    );
    await pool.query('ALTER TABLE respuestas_fijas ADD COLUMN IF NOT EXISTS pregunta TEXT');
    await pool.query('ALTER TABLE respuestas_fijas ADD COLUMN IF NOT EXISTS orden INTEGER NOT NULL DEFAULT 0');
    await cargarPreguntas();
}

// Las preguntas y respuestas se escriben en preguntas.json (no en la base).
// Cada vez que el servidor arranca (o sea, cada vez que se sube un cambio a GitHub),
// la tabla respuestas_fijas se reemplaza por lo que diga ese archivo.
// OJO: si se edita la tabla a mano en la base, se pisa en el próximo arranque.
async function cargarPreguntas() {
    const preguntas = require('./preguntas.json');
    const cliente = await pool.connect();
    try {
        await cliente.query('BEGIN');
        await cliente.query('DELETE FROM respuestas_fijas');
        for (let i = 0; i < preguntas.length; i++) {
            const { clave, pregunta, respuesta } = preguntas[i];
            await cliente.query(
                'INSERT INTO respuestas_fijas (pregunta_clave, pregunta, respuesta, orden) VALUES ($1, $2, $3, $4)',
                [clave, pregunta, respuesta, i]
            );
        }
        await cliente.query('COMMIT');
        console.log(`Preguntas cargadas: ${preguntas.length}`);
    } catch (err) {
        await cliente.query('ROLLBACK');
        throw err;
    } finally {
        cliente.release();
    }
}

prepararBase()
    .catch((err) => console.error('No se pudo preparar la base:', err))
    .finally(() => {
        app.listen(PORT, () => {
            console.log(`Servidor del Tamagotchi corriendo en http://localhost:${PORT}`);
        });
    });