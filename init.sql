-- Идемпотентная инициализация схемы IoT Platform.
-- Вызывается сервисом db-init при запуске Docker Compose.
-- IF NOT EXISTS создаёт отсутствующие таблицы, не пересоздавая существующие.

CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
    name TEXT,
    telegram_chat_id TEXT
);

ALTER TABLE users
    ADD COLUMN IF NOT EXISTS telegram_chat_id TEXT;

ALTER TABLE users
    DROP COLUMN IF EXISTS password;

ALTER TABLE users
    ADD COLUMN IF NOT EXISTS password_hash TEXT;

ALTER TABLE users
    ADD COLUMN IF NOT EXISTS salt TEXT;

UPDATE users
    SET password_hash = '', salt = ''
    WHERE password_hash IS NULL OR salt IS NULL;

ALTER TABLE users
    ALTER COLUMN password_hash SET NOT NULL,
    ALTER COLUMN salt SET NOT NULL;

CREATE UNIQUE INDEX IF NOT EXISTS users_telegram_chat_id_unique
    ON users (telegram_chat_id)
    WHERE telegram_chat_id IS NOT NULL;

-- Роль пользователя: admin или guest
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'user_status_enum') THEN
        CREATE TYPE user_status_enum AS ENUM ('admin', 'guest');
    END IF;
END
$$;

CREATE TABLE IF NOT EXISTS user_status (
    id SERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
    status user_status_enum NOT NULL DEFAULT 'guest'
);

-- Права доступа по роли (статусу): может ли выполнять GET/POST/PUT/DELETE
CREATE TABLE IF NOT EXISTS permissions (
    status user_status_enum PRIMARY KEY,
    can_get  BOOLEAN NOT NULL DEFAULT FALSE,
    can_post BOOLEAN NOT NULL DEFAULT FALSE,
    can_put  BOOLEAN NOT NULL DEFAULT FALSE,
    can_delete BOOLEAN NOT NULL DEFAULT FALSE
);

INSERT INTO permissions (status, can_get, can_post, can_put, can_delete)
VALUES
    ('admin', TRUE,  TRUE,  TRUE,  TRUE),
    -- Обычный пользователь управляет своими устройствами; чужые устройства
    -- защищены проверкой владельца в DevicesService.
    ('guest', TRUE,  TRUE,  TRUE,  TRUE)
ON CONFLICT (status) DO UPDATE SET
    can_get  = EXCLUDED.can_get,
    can_post = EXCLUDED.can_post,
    can_put  = EXCLUDED.can_put,
    can_delete = EXCLUDED.can_delete;

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
    time TIMESTAMP WITHOUT TIME ZONE DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC')
);

ALTER TABLE telemetry
    ALTER COLUMN time
    SET DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC');

-- Синхронизируем sequence с данными, чтобы ручные INSERT не ломали автоинкремент.
-- Для пустой таблицы следующий id должен быть 1 (is_called = false).
SELECT setval(pg_get_serial_sequence('users', 'id'),
              COALESCE((SELECT MAX(id) FROM users), 1), EXISTS(SELECT 1 FROM users));
SELECT setval(pg_get_serial_sequence('user_status', 'id'),
              COALESCE((SELECT MAX(id) FROM user_status), 1), EXISTS(SELECT 1 FROM user_status));
SELECT setval(pg_get_serial_sequence('devices', 'id'),
              COALESCE((SELECT MAX(id) FROM devices), 1), EXISTS(SELECT 1 FROM devices));
SELECT setval(pg_get_serial_sequence('rules', 'rule_id'),
              COALESCE((SELECT MAX(rule_id) FROM rules), 1), EXISTS(SELECT 1 FROM rules));
SELECT setval(pg_get_serial_sequence('telemetry', 'id'),
              COALESCE((SELECT MAX(id) FROM telemetry), 1), EXISTS(SELECT 1 FROM telemetry));
