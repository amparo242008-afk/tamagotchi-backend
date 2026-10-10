-- =============================================
-- Esquema del Tamagotchi
-- Ejecutar esto una sola vez en la base de datos.
-- =============================================

CREATE TABLE IF NOT EXISTS estado_mascota (
    id                   SERIAL PRIMARY KEY,
    nombre               TEXT        NOT NULL,
    hambre               NUMERIC(5,2) NOT NULL DEFAULT 100,
    sueno                NUMERIC(5,2) NOT NULL DEFAULT 100,
    dopamina             NUMERIC(5,2) NOT NULL DEFAULT 100,
    durmiendo            BOOLEAN      NOT NULL DEFAULT false,
    ultima_conexion      TIMESTAMPTZ,  -- última vez que Chaca habló con el servidor (para el panel)
    ultima_actualizacion TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- Cada accion que se hace (alimentar, jugar, dormir, despertar), con fecha.
-- Es la base del panel web para el acompañante.
CREATE TABLE IF NOT EXISTS historial_acciones (
    id    SERIAL PRIMARY KEY,
    tipo  TEXT        NOT NULL,
    fecha TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    clave TEXT  -- de qué rango de la agenda es (ej. "almuerzo"); vacío en las acciones del juguete
);

-- Cuántas veces preguntó Chaca en cada rango de la agenda, por día.
-- cantidad_insistencias = 3 y completado = false: se avisó el máximo y no hubo "sí".
CREATE TABLE IF NOT EXISTS seguimiento_avisos (
    id                    SERIAL PRIMARY KEY,
    tipo                  TEXT        NOT NULL,  -- clave del rango: desayuno, almuerzo, cena, bano, dormir
    fecha                 DATE        NOT NULL,  -- día en que empezó el rango
    cantidad_insistencias INTEGER     NOT NULL DEFAULT 0,
    ultima_insistencia    TIMESTAMPTZ,
    completado            BOOLEAN     NOT NULL DEFAULT FALSE,
    UNIQUE (tipo, fecha)
);

-- Ajustes que se cambian desde el panel del acompañante.
-- clave 'horarios' = los horarios de la rutina que cambió el acompañante (pisan los de rutina.json)
CREATE TABLE IF NOT EXISTS configuracion (
    clave TEXT PRIMARY KEY,
    valor JSONB NOT NULL
);

-- Notas que escribe el acompañante en el panel
CREATE TABLE IF NOT EXISTS notas_panel (
    id    SERIAL PRIMARY KEY,
    fecha TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    texto TEXT        NOT NULL
);

-- Se llena sola desde preguntas.json cada vez que arranca el servidor
CREATE TABLE IF NOT EXISTS respuestas_fijas (
    pregunta_clave TEXT PRIMARY KEY,
    pregunta       TEXT,
    respuesta      TEXT NOT NULL,
    orden          INTEGER NOT NULL DEFAULT 0
);

-- Mascota inicial (id = 1, que es la que leen los endpoints)
INSERT INTO estado_mascota (id, nombre, hambre, sueno, dopamina)
VALUES (1, 'Chaca', 100, 100, 100)
ON CONFLICT (id) DO NOTHING;

-- Las preguntas y respuestas ya no van aca: se editan en preguntas.json
