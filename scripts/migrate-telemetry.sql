-- Миграция: очистить БД, оставить только устройство #1 и его телеметрию.
-- Внимание: хеш пароля ниже посчитан с текущим PASSWORD_PEPPER из .env.
-- Если pepper менялся, сгенерируйте хеш заново (см. scripts/reset-db-keep-device1.sh).

BEGIN;

-- История телеметрии устройств, кроме #1.
DELETE FROM telemetry WHERE device_id <> 1;

-- Правила и их связи с устройствами.
DELETE FROM device_to_rule;
DELETE FROM rules;

-- Устройства, кроме #1 (их телеметрия уже удалена выше).
DELETE FROM devices WHERE id <> 1;

-- Пользователи, кроме сервисного пользователя #1.
DELETE FROM user_status WHERE user_id <> 1;
DELETE FROM users WHERE id <> 1;

-- Пользователь с рабочим паролем demo1234.
INSERT INTO users (id, email, password_hash, salt, name, telegram_chat_id)
VALUES (
    1,
    'demo@iot.local',
    '6f76306697f61a360611849c75c1ee6e7678ec5576f99b0ffb05cdab85d64a7f',
    '272c9f22610a7554c2fb2cbca440f06b',
    'Демо',
    NULL
)
ON CONFLICT (id) DO UPDATE SET
    email = EXCLUDED.email,
    password_hash = EXCLUDED.password_hash,
    salt = EXCLUDED.salt,
    name = EXCLUDED.name,
    telegram_chat_id = NULL;

-- Административные права для пользователя #1.
INSERT INTO user_status (user_id, status)
VALUES (1, 'admin')
ON CONFLICT (user_id) DO UPDATE SET status = 'admin';

-- Гарантируем, что устройство #1 существует и принадлежит пользователю #1.
INSERT INTO devices (id, owner_id, name, type)
VALUES (1, 1, 'Датчик кухни', 'temperature')
ON CONFLICT (id) DO UPDATE SET
    owner_id = 1,
    name = EXCLUDED.name,
    type = EXCLUDED.type;

COMMIT;
