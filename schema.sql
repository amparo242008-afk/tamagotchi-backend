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

-- Cuántas veces preguntó Roberto en cada rango de la agenda, por día.
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

-- Se llena sola desde preguntas.json cada vez que arranca el servidor
CREATE TABLE IF NOT EXISTS respuestas_fijas (
    pregunta_clave TEXT PRIMARY KEY,
    pregunta       TEXT,
    respuesta      TEXT NOT NULL,
    orden          INTEGER NOT NULL DEFAULT 0
);

-- Mascota inicial (id = 1, que es la que leen los endpoints)
INSERT INTO estado_mascota (id, nombre, hambre, sueno, dopamina)
VALUES (1, 'Roberto', 100, 100, 100)
ON CONFLICT (id) DO NOTHING;

-- Las preguntas y respuestas ya no van aca: se editan en preguntas.json
