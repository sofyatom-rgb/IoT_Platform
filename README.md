# IoT Platform

Небольшой IoT-бэкенд на C++ и Drogon с PostgreSQL. В проекте два HTTP-сервиса:

- **IoT Core** — основной API: пользователи, устройства и телеметрия. По умолчанию слушает порт `8080`.
- **HTTP Adapter** — входная точка для передачи и чтения телеметрии; пересылает запросы в Core. По умолчанию слушает порт `8081`.

## Требования

- CMake;
- C++-компилятор с поддержкой C++26;
- Drogon и его зависимости;
- Docker Compose.

## Сборка и запуск

Все команды ниже выполняются из корня репозитория, если отдельно не указано иное.

Соберите оба приложения:

```sh
cmake -S . -B build
cmake --build build
```

Запустите PostgreSQL:

```sh
docker compose up -d
```

При каждом запуске `docker compose up` Compose дождётся готовности PostgreSQL, затем сервис `db-init` применит `init.sql` и создаст отсутствующие таблицы `users`, `devices` и `telemetry`. Скрипт использует `CREATE TABLE IF NOT EXISTS`, поэтому существующие таблицы и данные не удаляются.

Запустите Core и Adapter в **двух отдельных терминалах**. Оба приложения нужно запускать из папки `build`, так как они загружают конфиги из `./bin/`:

```sh
cd build
./bin/iot_core
```

Во втором терминале:

```sh
cd build
./bin/http_adapter
```

Порты и настройки подключения задаются файлами:

- `iot_core/src/telemetry/config/HTTPConfig.json` — порт Core;
- `iot_core/src/telemetry/config/PostgreSQLConfig.json` — подключение Core к PostgreSQL;
- `http_adapter/src/config/AdapterConfig.json` — порт Adapter и URL Core.

PostgreSQL опубликован на `localhost:5433`; имя базы — `iot_platform`, пользователь — `postgres`, пароль для локальной разработки — `postgres` ([docker-compose.yml](./docker-compose.yml)).

## Общий порядок работы

Сначала создайте пользователя, затем устройство для этого пользователя и после этого отправляйте показания с `device_id`, полученным при создании устройства.

### 1. Создать пользователя

```sh
curl -X POST http://localhost:8080/users \
  -H 'Content-Type: application/json' \
  -d '{"email":"user@example.com","password":"secret","name":"Alex"}'
```

Тело запроса:

| Поле | Тип | Описание |
|---|---|---|
| `email` | строка | Email пользователя; должен быть уникальным |
| `password` | строка | Пароль |
| `name` | строка | Имя пользователя |

Пример успешного ответа:

```json
{"status":"ok","user_id":1}
```

Запомните `user_id`: он нужен при создании устройства. Сейчас пароль сохраняется в базе **как обычный текст**; API не предназначен для использования с реальными паролями или в production без реализации безопасного хеширования и аутентификации.

### 2. Создать устройство

Подставьте `user_id` из ответа выше:

```sh
curl -X POST http://localhost:8080/devices \
  -H 'Content-Type: application/json' \
  -d '{"owner_id":1,"name":"Living room sensor","type":"temperature"}'
```

Тело запроса:

| Поле | Тип | Описание |
|---|---|---|
| `owner_id` | целое число | ID существующего пользователя |
| `name` | строка | Название устройства |
| `type` | строка | Тип устройства |

Пример успешного ответа:

```json
{"status":"ok","device_id":1}
```

Запомните `device_id`: он используется для отправки, чтения и удаления телеметрии.

### 3. Записать показание телеметрии

**Напрямую в IoT Core:**

```sh
curl -X POST http://localhost:8080/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"temperature":23.5}'
```

**Через HTTP Adapter:**

```sh
curl -X POST http://localhost:8081/ingest \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"temperature":23.5}'
```

Тело обоих запросов содержит:

| Поле | Тип | Описание |
|---|---|---|
| `device_id` | целое число | ID существующего устройства |
| `temperature` | число | Значение температуры |

Core сохраняет показание и время записи. Adapter принимает `POST /ingest` и пересылает данные в Core. Успешный ответ Core:

```json
{"status":"ok"}
```

Успешный ответ Adapter:

```json
{"status":"ok","message":"Data forwarded to IoT Core"}
```

Каждый такой запрос **добавляет новую строку**, а не заменяет предыдущее показание.

### 4. Получить последние показания устройства

```sh
curl 'http://localhost:8080/telemetry?device_id=1'
```

Можно запросить через Adapter:

```sh
curl 'http://localhost:8081/telemetry?device_id=1'
```

Core возвращает массив максимум из 10 последних записей, от новых к старым:

```json
[
  {"id":15,"device_id":1,"temperature":23.5,"time":"2026-10-25 01:35:00"},
  {"id":14,"device_id":1,"temperature":22.9,"time":"2026-10-25 01:30:00"}
]
```

Adapter оборачивает этот массив:

```json
{
  "status":"ok",
  "data":[
    {"id":15,"device_id":1,"temperature":23.5,"time":"2026-10-25 01:35:00"}
  ]
}
```

### 5. Проверить доступность Core

```sh
curl http://localhost:8080/ping
```

Ответ: `pong`. Этот маршрут зарегистрирован в Core; у Adapter отдельного маршрута `/ping` нет.

## Все реализованные HTTP-методы

Все пользовательские методы находятся в IoT Core (`http://localhost:8080`), кроме приёма и проксирования телеметрии через Adapter (`http://localhost:8081`).

| Сервис | Метод и путь | Назначение |
|---|---|---|
| Core | `GET /ping` | Проверка доступности |
| Core | `POST /users` | Создать пользователя |
| Core | `DELETE /users?id=<user_id>` | Удалить пользователя |
| Core | `POST /devices` | Создать устройство |
| Core | `DELETE /devices?id=<device_id>` | Удалить устройство |
| Core | `POST /telemetry` | Добавить показание |
| Core | `GET /telemetry?device_id=<device_id>` | Получить до 10 последних показаний |
| Core | `DELETE /telemetry?device_id=<device_id>` | Удалить все показания устройства |
| Adapter | `POST /ingest` | Переслать показание в Core |
| Adapter | `GET /telemetry?device_id=<device_id>` | Получить телеметрию через Core |
| Adapter | `DELETE /telemetry?device_id=<device_id>` | Удалить телеметрию через Core |

Для методов `DELETE` передавайте ID в query-параметре. Например:

```sh
curl -X DELETE 'http://localhost:8080/telemetry?device_id=1'
curl -X DELETE 'http://localhost:8080/devices?id=1'
curl -X DELETE 'http://localhost:8080/users?id=1'
```

Удаление устройства каскадно удаляет его показания. Удаление пользователя каскадно удаляет его устройства и их показания.

## Как изменять и удалять уже сохранённые данные

В API **пока нет методов `PUT` или `PATCH`**: существующие данные пользователя или устройства нельзя изменить HTTP-запросом. Показание можно исправить напрямую в PostgreSQL, указав его `id`:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform \
  -c 'UPDATE telemetry SET temperature = 24.0 WHERE id = 15;'
```

Так же можно изменить данные устройства, например его название:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform \
  -c "UPDATE devices SET name = 'Kitchen sensor' WHERE id = 1;"
```

Будьте осторожны с `DELETE /telemetry`: он удаляет **все** показания указанного устройства, а не одну запись.

## Просмотр данных в PostgreSQL

Посмотреть таблицы:

```sh
docker compose exec postgres psql -U postgres -d iot_platform -c '\dt'
```

Посмотреть пользователей и устройства:

```sh
docker compose exec postgres psql -U postgres -d iot_platform \
  -c 'SELECT id, email, name FROM users ORDER BY id;'
docker compose exec postgres psql -U postgres -d iot_platform \
  -c 'SELECT id, owner_id, name, type FROM devices ORDER BY id;'
```

Посмотреть показания:

```sh
docker compose exec postgres psql -U postgres -d iot_platform \
  -c 'SELECT id, device_id, temperature, time FROM telemetry ORDER BY id DESC;'
```

## Повторное применение схемы

Текущий `init.sql` можно повторно применить вручную без удаления данных:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform < init.sql
```

Чтобы пересоздать таблицы с нуля для теста, удалите объекты схемы `public`, затем запустите Compose снова. **Данные будут потеряны**:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform \
  -c 'DROP SCHEMA public CASCADE; CREATE SCHEMA public;'
docker compose up -d
```

После запуска `db-init` автоматически создаст таблицы заново. `docker compose down` останавливает контейнер, но сохраняет данные в volume.

## Остановка

Остановить контейнер PostgreSQL, сохранив данные:

```sh
docker compose down
```
