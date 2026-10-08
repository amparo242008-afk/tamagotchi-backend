// server.js - Backend del Tamagotchi
// Node.js + Express + PostgreSQL

const express = require('express');
const { Pool } = require('pg');
require('dotenv').config();

const app = express();
app.use(express.json());

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
// Cuántos puntos baja cada stat por minuto que pasa
const DECAIMIENTO_POR_MINUTO = {
    hambre: 1,
    sueno: 0.5,
    dopamina: 1.5,
};

// Mientras Roberto duerme, el sueño SUBE (en vez de bajar) a este ritmo.
// Con 10 puntos por minuto, de 0 a 100 tarda 10 minutos.
const RECUPERACION_SUENO_POR_MINUTO = 10;

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
        if (accion.columna) {
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