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
-- Создать тип enum для операторов > < = !=
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'rule_operator') THEN
        CREATE TYPE rule_operator AS ENUM ('>', '<', '=', '!=');
    END IF;
END
$$;
-- Таблица правил (rule engine).
-- Правило — это само условие: оператор и пороговое значение.
-- Оно больше не привязано к конкретному устройству в этой таблице.
CREATE TABLE IF NOT EXISTS rules (
    rule_id SERIAL PRIMARY KEY,
    operator rule_operator NOT NULL,
    value DOUBLE PRECISION NOT NULL
);

-- Связующая таблица многие-ко-многим:
--   одному правилу может быть assigned(назначено) много устройств,
--   одному устройству может быть assigned(назначено) много правил.
--   device_to_rule.device_id -> devices.id
--   device_to_rule.rule_id  -> rules.rule_id
CREATE TABLE IF NOT EXISTS device_to_rule (
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    rule_id INTEGER NOT NULL REFERENCES rules(rule_id) ON DELETE CASCADE,
    PRIMARY KEY (device_id, rule_id)
);

CREATE TABLE IF NOT EXISTS telemetry (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    temperature REAL NOT NULL,
    time TIMESTAMP WITHOUT TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
