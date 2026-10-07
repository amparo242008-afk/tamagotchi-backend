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
    fecha TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS respuestas_fijas (
    pregunta_clave TEXT PRIMARY KEY,
    respuesta      TEXT NOT NULL
);

-- Mascota inicial (id = 1, que es la que leen los endpoints)
INSERT INTO estado_mascota (id, nombre, hambre, sueno, dopamina)
VALUES (1, 'Roberto', 100, 100, 100)
ON CONFLICT (id) DO NOTHING;

INSERT INTO respuestas_fijas (pregunta_clave, respuesta) VALUES
    ('nombre',        'Me llamo Roberto y soy un Tamagotchi.'),
    ('juego_favorito','Mi juego favorito es jugar conmigo mismo.'),
    ('comida_favorita','Me encanta que me alimentes.'),
    ('edad',          'Tengo todos los dias contigo.')
ON CONFLICT (pregunta_clave) DO NOTHING;
