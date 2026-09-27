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
    const sueno = Math.max(0, registro.sueno - minutosPasados * DECAIMIENTO_POR_MINUTO.sueno);
    const dopamina = Math.max(0, registro.dopamina - minutosPasados * DECAIMIENTO_POR_MINUTO.dopamina);

    return {
        nombre: registro.nombre,
        hambre: Math.round(hambre),
        sueno: Math.round(sueno),
        dopamina: Math.round(dopamina),
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
// Body esperado: { "tipo": "alimentar" | "jugar" | "dormir" }
// Sube el stat correspondiente y actualiza la marca de tiempo
app.post('/accion', async (req, res) => {
    const { tipo } = req.body;

    // Mapeo de acción -> qué columna sube y cuánto
    const acciones = {
        alimentar: { columna: 'hambre', suma: 30 },
        jugar: { columna: 'dopamina', suma: 30 },
        dormir: { columna: 'sueno', suma: 40 },
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

        const nuevoValor = Math.min(100, estadoActual[accion.columna] + accion.suma);

        await pool.query(
            `UPDATE estado_mascota
             SET ${accion.columna} = $1, ultima_actualizacion = NOW()
             WHERE id = 1`,
            [nuevoValor]
        );

        res.json({ mensaje: `${tipo} aplicado`, [accion.columna]: nuevoValor });
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
// 7. LEVANTAR EL SERVER
// ---------------------------------------------
app.listen(PORT, () => {
    console.log(`Servidor del Tamagotchi corriendo en http://localhost:${PORT}`);
});