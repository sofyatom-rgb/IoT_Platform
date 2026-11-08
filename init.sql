-- Идемпотентная инициализация схемы IoT Platform.
-- Вызывается сервисом db-init при запуске Docker Compose.
-- IF NOT EXISTS создаёт отсутствующие таблицы, не пересоздавая существующие.

CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    password TEXT NOT NULL,
    name TEXT,
    telegram_chat_id TEXT
);

ALTER TABLE users
    ADD COLUMN IF NOT EXISTS telegram_chat_id TEXT;

CREATE UNIQUE INDEX IF NOT EXISTS users_telegram_chat_id_unique
    ON users (telegram_chat_id)
    WHERE telegram_chat_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS devices (
    id SERIAL PRIMARY KEY,
    owner_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    type TEXT NOT NULL
);

-- Таблица правил для устройств (rule engine)
-- device_id ссылается на устройство, которому принадлежит правило
CREATE TABLE IF NOT EXISTS rules (
    rule_id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    operator TEXT NOT NULL CHECK (operator IN ('>', '<', '=', '!=')),
    value DOUBLE PRECISION NOT NULL
);

CREATE TABLE IF NOT EXISTS telemetry (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    temperature REAL NOT NULL,
    time TIMESTAMP WITHOUT TIME ZONE DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC')
);

ALTER TABLE telemetry
    ALTER COLUMN time
    SET DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC');
