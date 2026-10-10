// panel.js - Panel web del acompañante (todo lo que empieza con /panel)
//
// Qué hay acá:
//   1. Contraseña: todo /panel pide la clave PANEL_CLAVE (se configura en Render, Environment)
//   2. GET  /panel                 -> la página (panel/index.html)
//   3. GET  /panel/datos           -> lo que muestra la pestaña Seguimiento (hoy, grilla, historial)
//                                     con ?demo=1 usa datos inventados (para mostrar sin datos reales)
//   4. GET/POST/DELETE /panel/rutina -> ver y cambiar los horarios de la rutina
//   5. GET/POST/DELETE /panel/notas  -> las notas del acompañante
//
// Las cuentas (qué pasó en cada rango cada día) están en calcularPanel(), que recibe
// las filas de la base. Los datos de ejemplo (generarDemo) arman filas falsas con la
// misma forma, así el ejemplo pasa por exactamente las mismas cuentas que lo real.

const path = require('path');

const NOMBRES = { desayuno: 'Desayuno', almuerzo: 'Almuerzo', cena: 'Cena', bano: 'Baño', dormir: 'Dormir' };
const AREAS = { comida: 'Comida', higiene: 'Higiene', dormir: 'Descanso' };
const MIN = 60 * 1000;
const HORA = 60 * MIN;
const DIA = 24 * HORA;

module.exports = function (app, { pool, leerRutina, aMinutos, OFFSET_ARGENTINA_MS }) {
    // ---- Fechas en hora argentina ----
    const fechaAr = (ms) => new Date(ms + OFFSET_ARGENTINA_MS).toISOString().slice(0, 10);  // "2026-10-10"
    const horaAr = (ms) => new Date(ms + OFFSET_ARGENTINA_MS).toISOString().slice(11, 16);  // "13:05"
    // Medianoche argentina del día "2026-10-10", en ms reales
    const medianoche = (fecha) => Date.parse(fecha + 'T00:00:00Z') - OFFSET_ARGENTINA_MS;

    // =====================================================================
    // 1. CONTRASEÑA
    // =====================================================================
    // Usa la "autenticación básica" del navegador: la primera vez aparece un cartelito
    // pidiendo usuario y contraseña (el usuario puede ser cualquiera) y el navegador lo recuerda.
    function pedirClave(req, res, next) {
        const clave = process.env.PANEL_CLAVE;
        if (!clave) return res.status(503).send('Falta configurar PANEL_CLAVE en Render (Environment).');
        const [tipo, dato] = (req.headers.authorization || '').split(' ');
        if (tipo === 'Basic' && dato) {
            const escrita = Buffer.from(dato, 'base64').toString().split(':').slice(1).join(':');
            if (escrita === clave) return next();
        }
        res.set('WWW-Authenticate', 'Basic realm="Panel de Chaca", charset="UTF-8"');
        res.status(401).send('Hace falta la contraseña del panel.');
    }
    app.use('/panel', pedirClave);

    // =====================================================================
    // 2. LA PÁGINA
    // =====================================================================
    app.get('/panel', (req, res) => res.sendFile(path.join(__dirname, 'panel', 'index.html')));

    // =====================================================================
    // 3. DATOS DEL SEGUIMIENTO
    // =====================================================================
    app.get('/panel/datos', async (req, res) => {
        const dias = req.query.dias === '30' ? 30 : 14;
        try {
            const rutina = await leerRutina();
            const ahora = Date.now();
            const desde = medianoche(fechaAr(ahora)) - (dias - 1) * DIA;
            let filas;
            if (req.query.demo === '1') {
                filas = generarDemo(rutina, dias, ahora);
            } else {
                const [hist, seg, est, primero] = await Promise.all([
                    pool.query('SELECT tipo, clave, fecha FROM historial_acciones WHERE fecha >= $1 ORDER BY fecha',
                        [new Date(desde)]),
                    pool.query(`SELECT tipo AS clave, fecha::text AS fecha, cantidad_insistencias AS avisos
                                FROM seguimiento_avisos WHERE fecha >= $1::date`, [fechaAr(desde)]),
                    pool.query('SELECT ultima_conexion FROM estado_mascota WHERE id = 1'),
                    pool.query('SELECT MIN(fecha) AS fecha FROM historial_acciones'),
                ]);
                filas = {
                    historial: hist.rows.map((h) => ({ tipo: h.tipo, clave: h.clave, ms: new Date(h.fecha).getTime() })),
                    seguimiento: seg.rows,
                    ultimaConexion: est.rows[0]?.ultima_conexion ? new Date(est.rows[0].ultima_conexion).getTime() : null,
                    primerRegistro: primero.rows[0]?.fecha ? new Date(primero.rows[0].fecha).getTime() : null,
                };
            }
            res.json({ demo: req.query.demo === '1', ...calcularPanel(rutina, dias, ahora, filas) });
        } catch (err) {
            console.error(err);
            res.status(500).json({ error: 'Error al armar el panel' });
        }
    });

    // Con las filas de la base arma todo lo que muestra la pestaña Seguimiento
    function calcularPanel(rutina, dias, ahora, { historial, seguimiento, ultimaConexion, primerRegistro }) {
        const rangos = rutina.rangos.map((r) => {
            const desde = aMinutos(r.desde);
            const duracion = ((aMinutos(r.hasta) - desde + 1440) % 1440) * MIN;
            return {
                clave: r.clave, actividad: r.actividad, desde: r.desde, hasta: r.hasta, inicio: desde * MIN, duracion,
                nombre: NOMBRES[r.clave] || r.clave, area: AREAS[r.actividad] || r.actividad,
            };
        });
        const hoy = fechaAr(ahora);
        const fechas = [];
        for (let i = dias - 1; i >= 0; i--) fechas.push(fechaAr(medianoche(hoy) - i * DIA + 12 * HORA));

        // Lo que pasó en un rango un día. Todo lo anotado en las 24 h desde que empezó
        // el rango cuenta para ese día (así el "dormir" de las 00:30 es de la noche anterior).
        function vuelta(r, fecha) {
            const empieza = medianoche(fecha) + r.inicio;
            const termina = empieza + r.duracion;
            const ev = historial.filter((h) => h.clave === r.clave && h.ms >= empieza && h.ms < empieza + DIA);
            const busca = (prefijo) => ev.find((h) => h.tipo.startsWith(prefijo));
            const confirmo = busca('confirmo_');
            const pospuso = ev.find((h) => h.tipo.startsWith('posterga_') && (!confirmo || h.ms < confirmo.ms));
            const paso = busca('paso_');
            const sin = busca('sin_respuesta_');
            const avisos = seguimiento.find((s) => s.clave === r.clave && s.fecha === fecha)?.avisos || 0;
            let estado;
            if (confirmo) estado = pospuso ? 'ok2' : 'ok';
            else if (paso) estado = 'paso';
            else if (sin) estado = 'sin';
            else if (ahora < empieza) estado = 'nada';
            else if (ahora < termina) estado = 'espera';
            else estado = avisos > 0 ? 'sin' : 'sin_datos';  // sin avisos = Chaca estuvo apagado o sin internet
            return { estado, empieza, confirmo, pospuso, paso, sin, avisos };
        }

        const grilla = {};
        for (const r of rangos) grilla[r.clave] = fechas.map((f) => vuelta(r, f).estado);

        // ---- Hoy: una línea por rango ----
        const hoyDetalle = {};
        for (const r of rangos) {
            const v = vuelta(r, hoy);
            const partes = [];
            if (v.pospuso) partes.push(`Pospuso a las ${horaAr(v.pospuso.ms)}`);
            if (v.confirmo) {
                const micro = v.confirmo.tipo.startsWith('confirmo_micro_') ? ' (con una micro-tarea)' : '';
                partes.push(`${v.pospuso ? 'confirmó' : 'Confirmó'} a las ${horaAr(v.confirmo.ms)}${micro}`);
            } else if (v.paso) partes.push(`Eligió “Hoy paso” a las ${horaAr(v.paso.ms)}`);
            else if (v.sin) partes.push('Sin respuesta');
            else if (v.estado === 'nada') partes.push(`Empieza a las ${r.desde}`);
            else if (v.estado === 'espera' && !v.pospuso) partes.push(v.avisos ? 'Esperando respuesta' : 'Chaca todavía no preguntó');
            else if (v.estado === 'sin_datos') partes.push('Sin datos (Chaca apagado o sin internet)');
            hoyDetalle[r.clave] = { estado: v.estado, texto: partes.join(' · '), avisos: v.avisos };
        }

        // ---- Historial de hoy (lo más nuevo arriba) ----
        const nombreDe = (clave) => NOMBRES[clave] || clave;
        const JUGUETE = {
            alimentar: 'Le dio de comer a Chaca', jugar: 'Jugó a Atrapar con Chaca', dormir: 'Acostó a Chaca a dormir',
            despertar: 'Chaca se despertó', banar: 'Bañó a Chaca',
        };
        const textoHist = (h) => {
            if (JUGUETE[h.tipo]) return { texto: JUGUETE[h.tipo], juego: true };
            const n = nombreDe(h.clave);
            if (h.tipo.startsWith('confirmo_micro_')) return { texto: `${n}: confirmó con una micro-tarea` };
            if (h.tipo.startsWith('confirmo_')) return { texto: `${n}: confirmó` };
            if (h.tipo.startsWith('posterga_')) return { texto: `${n}: eligió “Más tarde”` };
            if (h.tipo.startsWith('paso_')) return { texto: `${n}: eligió “Hoy paso”` };
            if (h.tipo.startsWith('sin_respuesta_')) return { texto: `${n}: terminó el horario sin respuesta` };
            return { texto: h.tipo };
        };
        const historialHoy = historial
            .filter((h) => h.ms >= medianoche(hoy))
            .sort((a, b) => b.ms - a.ms)
            .map((h) => ({ hora: horaAr(h.ms), ...textoHist(h) }));

        // ---- Semáforo (lo que decidió la acompañante) ----
        // Atención moderada: un área sin ninguna respuesta (sí, más tarde u hoy paso) hace 2 días o más.
        // Atención: ninguna respuesta en nada hace más de 3 días.
        const respondio = (h) => /^(confirmo|posterga|paso)_/.test(h.tipo);
        const areas = [...new Set(rangos.map((r) => r.area))];
        const ultimaRespuesta = {};
        for (const a of areas) {
            const claves = rangos.filter((r) => r.area === a).map((r) => r.clave);
            const ultimas = historial.filter((h) => respondio(h) && claves.includes(h.clave)).map((h) => h.ms);
            ultimaRespuesta[a] = ultimas.length ? Math.max(...ultimas) : null;
        }
        const desdeHace = (ms) => (ms === null ? Infinity : ahora - ms);
        let semaforo;
        if (primerRegistro === null) {
            semaforo = { nivel: 'sin_datos', texto: 'Todavía no hay registros. Cuando Chaca empiece a preguntar, acá va a aparecer el resumen.' };
        } else {
            const inicioUso = ahora - primerRegistro;  // no alarmar si recién se empezó a usar
            const flojas = areas.filter((a) => desdeHace(ultimaRespuesta[a]) >= 2 * DIA && inicioUso >= 2 * DIA);
            const diasSin = (a) => Math.floor(Math.min(desdeHace(ultimaRespuesta[a]), inicioUso) / DIA);
            if (flojas.length === areas.length && areas.every((a) => desdeHace(ultimaRespuesta[a]) > 3 * DIA) && inicioUso > 3 * DIA) {
                semaforo = { nivel: 'atencion', texto: `Sin ninguna respuesta hace ${Math.min(...areas.map(diasSin))} días.` };
            } else if (flojas.length) {
                const bien = areas.filter((a) => !flojas.includes(a));
                semaforo = {
                    nivel: 'moderada',
                    texto: flojas.map((a) => `${a} sin respuesta hace ${diasSin(a)} días.`).join(' ')
                        + (bien.length ? ` ${bien.join(' y ')}: normal.` : ''),
                };
            } else {
                semaforo = { nivel: 'normal', texto: 'Todas las áreas con respuestas en los últimos 2 días.' };
            }
        }

        // ---- Resumen por área (solo rangos ya terminados y con datos) ----
        const resumen = areas.map((a) => {
            let si = 0, total = 0;
            for (const r of rangos.filter((x) => x.area === a))
                for (const e of grilla[r.clave]) {
                    if (['ok', 'ok2', 'sin', 'paso'].includes(e)) total++;
                    if (e === 'ok' || e === 'ok2') si++;
                }
            return { area: a, si, total };
        });

        return {
            hoy,
            ahora,
            ultimaConexion,
            maxAvisos: rutina.max_avisos,
            rangos: rangos.map(({ clave, nombre, area, desde, hasta }) => ({ clave, nombre, area, desde, hasta })),
            fechas,
            grilla,
            hoyDetalle,
            historial: historialHoy,
            semaforo,
            resumen,
        };
    }

    // ---- Datos de ejemplo ----
    // Inventa filas con la misma forma que las de la base: casi todo confirmado,
    // un "bajón" de 3 días en el medio y el baño sin respuesta los últimos días,
    // para que se vea cómo el panel muestra un patrón. Siempre da lo mismo (no es al azar).
    function generarDemo(rutina, dias, ahora) {
        let semilla = 7;
        const azar = () => ((semilla = (semilla * 9301 + 49297) % 233280) / 233280);
        const historial = [];
        const seguimiento = [];
        const hoy = fechaAr(ahora);
        for (let i = dias - 1; i >= 0; i--) {
            const fecha = fechaAr(medianoche(hoy) - i * DIA + 12 * HORA);
            const bajon = i >= 6 && i <= 8;
            for (const r of rutina.rangos) {
                const empieza = medianoche(fecha) + aMinutos(r.desde) * MIN;
                const termina = empieza + ((aMinutos(r.hasta) - aMinutos(r.desde) + 1440) % 1440) * MIN;
                if (empieza > ahora) continue;
                const p = azar();
                let resultado = bajon ? (p < 0.6 ? 'sin' : 'ok2') : p < 0.65 ? 'ok' : p < 0.88 ? 'ok2' : p < 0.95 ? 'paso' : 'sin';
                if (r.clave === 'bano' && i <= 2) resultado = 'sin';
                const anotar = (tipo, ms) => { if (ms <= ahora) historial.push({ tipo: `${tipo}_${r.actividad}`, clave: r.clave, ms }); };
                const minuto = (m) => empieza + m * MIN + Math.floor(azar() * 20) * MIN;
                let avisos = 1;
                if (resultado === 'ok') anotar('confirmo', minuto(2));
                if (resultado === 'ok2') { anotar('posterga', minuto(1)); anotar('confirmo', minuto(46)); avisos = 2; }
                if (resultado === 'paso') { anotar('posterga', minuto(1)); anotar('paso', minuto(46)); avisos = 2; }
                if (resultado === 'sin') { avisos = Math.min(3, Math.floor((ahora - empieza) / (45 * MIN)) + 1); anotar('sin_respuesta', termina + MIN); }
                seguimiento.push({ clave: r.clave, fecha, avisos });
            }
            // un poco de juego con Chaca
            const tarde = medianoche(fecha) + 16 * HORA + Math.floor(azar() * 120) * MIN;
            if (tarde <= ahora && azar() < 0.7) historial.push({ tipo: 'jugar', clave: null, ms: tarde });
            if (tarde + 2 * HORA <= ahora && azar() < 0.5) historial.push({ tipo: 'alimentar', clave: null, ms: tarde + 2 * HORA });
        }
        historial.sort((a, b) => a.ms - b.ms);
        return { historial, seguimiento, ultimaConexion: ahora - 12 * MIN, primerRegistro: ahora - 60 * DIA };
    }

    // =====================================================================
    // 4. RUTINA: ver y cambiar los horarios
    // =====================================================================
    const HHMM = /^([01]\d|2[0-3]):[0-5]\d$/;

    app.get('/panel/rutina', async (req, res) => {
        try {
            const rutina = await leerRutina();
            const cambiada = (await pool.query("SELECT 1 FROM configuracion WHERE clave = 'horarios'")).rows.length > 0;
            res.json({
                cambiada,
                silencio: rutina.silencio,
                rangos: rutina.rangos.map((r) => ({
                    clave: r.clave, nombre: NOMBRES[r.clave] || r.clave, pregunta: r.pregunta, desde: r.desde, hasta: r.hasta,
                })),
            });
        } catch (err) {
            console.error(err);
            res.status(500).json({ error: 'Error al leer la rutina' });
        }
    });

    // Body: { silencio: { desde, hasta }, rangos: { desayuno: { desde, hasta }, ... } }
    // Solo se guardan los horarios; las frases y micro-tareas siguen saliendo de rutina.json.
    app.post('/panel/rutina', async (req, res) => {
        const { silencio, rangos } = req.body || {};
        const valido = (h) => h && HHMM.test(h.desde) && HHMM.test(h.hasta) && h.desde !== h.hasta;
        try {
            const base = await leerRutina();
            if (!valido(silencio)) return res.status(400).json({ error: 'El horario de silencio no es válido.' });
            const limpio = { silencio: { desde: silencio.desde, hasta: silencio.hasta }, rangos: {} };
            for (const r of base.rangos) {
                const h = rangos?.[r.clave];
                if (!valido(h)) {
                    return res.status(400).json({ error: `El horario de ${NOMBRES[r.clave] || r.clave} no es válido (el inicio y el fin no pueden ser iguales).` });
                }
                limpio.rangos[r.clave] = { desde: h.desde, hasta: h.hasta };
            }
            await pool.query(
                `INSERT INTO configuracion (clave, valor) VALUES ('horarios', $1)
                 ON CONFLICT (clave) DO UPDATE SET valor = $1`,
                [limpio]
            );
            res.json({ mensaje: 'Rutina actualizada' });
        } catch (err) {
            console.error(err);
            res.status(500).json({ error: 'No se pudo guardar la rutina' });
        }
    });

    // Volver a los horarios de rutina.json
    app.delete('/panel/rutina', async (req, res) => {
        try {
            await pool.query("DELETE FROM configuracion WHERE clave = 'horarios'");
            res.json({ mensaje: 'Se volvió a la rutina original' });
        } catch (err) {
            console.error(err);
            res.status(500).json({ error: 'No se pudo volver a la rutina original' });
        }
    });

    // =====================================================================
    // 5. NOTAS DEL ACOMPAÑANTE
    // =====================================================================
    app.get('/panel/notas', async (req, res) => {
        try {
            const result = await pool.query('SELECT id, fecha, texto FROM notas_panel ORDER BY fecha DESC');
            res.json(result.rows);
        } catch (err) {
            console.error(err);
            res.status(500).json({ error: 'Error al leer las notas' });
        }
    });

    app.post('/panel/notas', async (req, res) => {
        const texto = String(req.body?.texto || '').trim();
        if (!texto || texto.length > 2000) return res.status(400).json({ error: 'La nota está vacía o es muy larga.' });
        try {
            const result = await pool.query('INSERT INTO notas_panel (texto) VALUES ($1) RETURNING id, fecha, texto', [texto]);
            res.json(result.rows[0]);
        } catch (err) {
            console.error(err);
            res.status(500).json({ error: 'No se pudo guardar la nota' });
        }
    });

    app.delete('/panel/notas/:id', async (req, res) => {
        try {
            await pool.query('DELETE FROM notas_panel WHERE id = $1', [Number(req.params.id)]);
            res.json({ mensaje: 'Nota borrada' });
        } catch (err) {
            console.error(err);
            res.status(500).json({ error: 'No se pudo borrar la nota' });
        }
    });
};
