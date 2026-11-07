# IoT Platform

IoT Platform — учебный backend для регистрации пользователей и устройств, приёма температурных показаний, чтения телеметрии и отправки уведомлений в Telegram. Сервисы написаны на C++ с Drogon, данные хранятся в PostgreSQL.

В проекте есть три HTTP-сервиса:

1. **IoT Core** — основной сервис и единственный компонент, который работает с PostgreSQL. Адрес по умолчанию: `http://localhost:8080`.
2. **HTTP Adapter** — дополнительный HTTP-вход для интеграций и IoT-клиентов. Адрес по умолчанию: `http://localhost:8081`. Он принимает телеметрию и проксирует запросы к IoT Core.
3. **Notification Service** — внутренний HTTP-сервис, принимающий события от Core и отправляющий уведомления через Telegram Bot API и/или Gmail SMTP (email). Адрес по умолчанию: `http://localhost:8082`.

## Содержание

- [Как устроен проект](#как-устроен-проект)
- [Поток данных](#поток-данных)
- [Требования](#требования)
- [Сборка](#сборка)
- [Запуск и автоматическая инициализация базы](#запуск-и-автоматическая-инициализация-базы)
- [Общий сценарий использования](#общий-сценарий-использования)
- [HTTP API](#http-api)
- [Изменение и удаление данных](#изменение-и-удаление-данных)
- [Работа с PostgreSQL вручную](#работа-с-postgresql-вручную)
- [Остановка и сброс базы](#остановка-и-сброс-базы)
- [Ограничения и безопасность](#ограничения-и-безопасность)

## Как устроен проект

```text
IoT_Platform/
├── CMakeLists.txt                    # Корневая сборка: C++26, Drogon и приложения
├── docker-compose.yml                # PostgreSQL, проверка готовности, автоматический db-init
├── init.sql                          # Создание таблиц и внешних ключей
├── README.md                         # Это руководство
│
├── iot_core/                         # Основной HTTP API и доступ к данным
│   ├── CMakeLists.txt                # Цель сборки iot_core и копирование конфигов
│   ├── config/
│   │   ├── HTTPConfig.json           # Адрес и порт Core (по умолчанию 8080)
│   │   └── PostgreSQLConfig.json    # Параметры подключения к БД
│   └── src/
│       ├── main.cpp                  # Загружает конфиги, регистрирует контроллеры, запускает Core
│       ├── telemetry/
│       │   ├── controllers/
│       │   │   └── TelemetryController.cpp # HTTP-маршруты телеметрии и /ping
│       │   ├── services/
│       │   │   └── TelemetryService.cpp # Слой бизнес-логики телеметрии
│       │   ├── clients/
│       │   │   └── NotificationServiceClient.cpp # HTTP-клиент для Notification Service (chat_id + email)
│       │   └── repositories/
│       │       └── TelemetryRepository.cpp # SQL для таблицы telemetry
│       ├── users/
│       │   ├── controllers/
│       │   │   └── UsersController.cpp # HTTP-маршруты пользователей
│       │   ├── services/
│       │   │   └── UsersService.cpp  # Слой бизнес-логики пользователей
│       │   └── repositories/
│       │       └── UsersRepository.cpp # SQL для таблицы users
│       └── devices/
│           ├── controllers/
│           │   └── DevicesController.cpp # HTTP-маршруты устройств
│           ├── services/
│           │   └── DevicesService.cpp # Слой бизнес-логики устройств
│           └── repositories/
│               └── DevicesRepository.cpp # SQL для таблицы devices
│
├── http_adapter/                     # HTTP-шлюз к IoT Core
│   ├── CMakeLists.txt                # Цель сборки http_adapter и копирование конфига
│   └── src/
│       ├── main.cpp                  # Загружает конфиг, регистрирует маршруты, запускает Adapter
│       ├── config/
│       │   └── AdapterConfig.json    # Порт Adapter и URL IoT Core
│       └── ingest/
│           ├── controllers/
│           │   └── IngestController.cpp # Входные HTTP-маршруты Adapter
│           ├── services/
│           │   └── AdapterService.cpp # Координирует проксирование запросов
│           └── clients/
│               └── IoTClient.cpp    # HTTP-клиент для вызова IoT Core
│
├── notification_service/             # Отдельный HTTP-сервис Telegram-уведомлений
│   ├── CMakeLists.txt
│   ├── config/HTTPConfig.json        # HTTP-порт 8082
│   └── src/
│       ├── main.cpp
│       ├── controllers/              # Принимает внутренние запросы от Core
│       ├── services/                 # Формирует содержание уведомления
│       ├── clients/                  # Telegram (tgbot-cpp)
│       ├── notifications/            # Email config, SMTP service, templates
│       └── resources/email/          # HTML и текстовые шаблоны писем
│
└── build/                            # Генерируется CMake; здесь находятся бинарники
    └── bin/
        ├── iot_core
        ├── http_adapter
        ├── notification_service
        └── *.json                    # Копии конфигов, загружаемые приложениями
```

### Роли слоёв

- **Controller** разбирает HTTP-запрос, проверяет входной JSON и формирует HTTP-ответ.
- **Service** передаёт операцию репозиторию. Здесь располагается бизнес-логика.
- **Repository** выполняет SQL-запросы через Drogon ORM.
- В Adapter контроллер вызывает сервис, сервис использует HTTP-клиент, а HTTP-клиент обращается к Core.
- Core вызывает Notification Service через `NotificationServiceClient`; сервис уведомлений параллельно отправляет в настроенные каналы: Telegram (`TelegramBotClient`, tgbot-cpp) и Email (`EmailService`, Gmail SMTP через Mailio с TLS и MIME `multipart/alternative`). Блокирующие вызовы выполняются в общем пуле потоков, event loop Drogon не блокируется.

В Core данные из SQL привязываются параметрами (`$1`, `$2`, …), а не подставляются конкатенацией в SQL-строку.

## Поток данных

```text
IoT-устройство / curl / клиентское приложение
                  │
                  ├── напрямую: HTTP :8080
                  │              │
                  │              ▼
                  │       IoT Core Controller
                  │              │
                  │       Service → Repository
                  │              │
                  │              ▼
                  │         PostgreSQL
                  │
                  └── через Adapter: HTTP :8081
                                 │
                                 ▼
                         Adapter Controller
                                 │
                       Adapter Service → IoTClient
                                 │ HTTP-запрос на :8080
                                 └──────────────► IoT Core
                                                      │
                                  если температура выше порога
                                                      ▼
                                          NotificationServiceClient
                                                      │ HTTP :8082
                                                      ▼
                                           Notification Controller
                                                      │
                                   Notification Service ─┬─→ TelegramBotClient → Telegram Bot API
                                                         └─→ EmailService (Mailio) → Gmail SMTP
```

### Когда обращаться напрямую, а когда через Adapter

**Напрямую в Core** удобно для локальной разработки, тестирования API и внутренних клиентов, которым доступен порт Core. Core предоставляет операции пользователей, устройств и телеметрии.

**Через Adapter** удобно направлять телеметрию от внешних клиентов через отдельную точку входа. Adapter отделяет внешний ingest API от внутреннего Core и может служить местом для будущей интеграционной логики. Сейчас он пересылает телеметрию без дополнительной обработки; пользователей и устройства он не обслуживает.

Оба пути записи сохраняют данные в одну и ту же базу: Adapter не имеет собственного подключения к PostgreSQL. Если нужен только один процесс при локальной проверке, запросы телеметрии можно направлять напрямую в Core; для работы Adapter должны быть запущены оба сервиса.

Если превышен температурный порог, Core запрашивает у владельца устройства контакты для уведомлений (Telegram `chat_id` и email) из PostgreSQL, затем отправляет событие в Notification Service. Тот параллельно пытается доставить уведомление в каждый настроенный канал. Ошибка доставки логируется отдельно и не отменяет уже сохранённую телеметрию.

## Требования

- Linux/macOS или другая среда, где доступны инструменты проекта;
- CMake версии 3.30 или новее;
- C++-компилятор с поддержкой C++26;
- установленный Drogon и его зависимости;
- установленная библиотека [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp) и её зависимости: Boost (headers), libcurl, nlohmann_json;
- Boost 1.81+ (`date_time`, `regex`) и OpenSSL;
- Mailio 0.26.1 загружается CMake из официального репозитория при конфигурации;
- Docker и Docker Compose plugin;
- `curl` для примеров ниже (необязательно, можно использовать любой HTTP-клиент).

Проект находит Drogon, TgBot, Boost и OpenSSL через CMake; Mailio загружается по зафиксированной версии при конфигурации.

На Debian/Ubuntu зависимости tgbot-cpp ставятся так:

```sh
sudo apt install -y libboost-dev libcurl4-openssl-dev nlohmann-json3-dev
```

Затем соберите и установите саму библиотеку:

```sh
git clone --depth 1 https://github.com/reo7sp/tgbot-cpp.git
cmake -S tgbot-cpp -B tgbot-cpp/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build tgbot-cpp/build -j
sudo cmake --install tgbot-cpp/build
```

## Сборка

Команды выполняются из корня репозитория:

```sh
cmake -S . -B build
cmake --build build
```

CMake собирает цели:

- `iot_core`;
- `http_adapter`;
- `notification_service`;
- `codec_cli` (утилита для работы с кодеком Adapter).

Бинарники создаются в `build/bin/`, копии конфигурационных файлов — в `build/config/`. После изменения исходников пересоберите проект:

```sh
cmake --build build
```

Для чистой генерации файлов CMake можно указать другой пустой каталог, например `build-clean`:

```sh
cmake -S . -B build-clean
cmake --build build-clean
```

## Запуск и автоматическая инициализация базы

### 1. Запустить контейнеры и дождаться базы

Из корня проекта:

```sh
docker compose up -d
```

В Compose описаны два сервиса:

- `postgres` запускает PostgreSQL 16 и сохраняет данные в Docker volume `postgres_data`;
- `db-init` ждёт успешного healthcheck PostgreSQL, затем запускает `psql` с файлом `init.sql`.

То есть схема инициализируется автоматически при `docker compose up`; вручную выполнять `psql` для обычного запуска не требуется. Это работает и с уже созданным volume. Скрипт `init.sql` использует `CREATE TABLE IF NOT EXISTS`: он создаёт отсутствующие таблицы и не удаляет существующие строки.

Успешная инициализация `db-init` завершается кодом `0`. Это одноразовый контейнер-задача, поэтому после запуска он может отображаться как `Exited (0)` — это нормально. Проверить таблицы можно командой из раздела [Работа с PostgreSQL вручную](#работа-с-postgresql-вручную).

Параметры локальной БД заданы в `docker-compose.yml`:

| Параметр | Значение |
|---|---|
| Хост для приложений на компьютере | `127.0.0.1` |
| Опубликованный порт | `5433` |
| Порт PostgreSQL внутри Compose-сети | `5432` |
| База | `iot_platform` |
| Пользователь | `postgres` |
| Пароль | `postgres` |

Core подключается к БД на `127.0.0.1:5433` согласно `iot_core/config/PostgreSQLConfig.json`. У Adapter соединения с БД нет.

### 2. Запустить IoT Core

Core ищет конфиги рядом с исполняемым файлом, поэтому его можно запускать из корня проекта:

```sh
./build/bin/iot_core
```

Ожидаемый порт по умолчанию: `8080`.

### 3. Запустить Notification Service

Сервис поддерживает два канала уведомлений: **Telegram** и **Email через SMTP**. Можно включить любой из них или оба.

**Telegram (опционально).** Создайте бота через BotFather и задайте токен в окружении (не добавляйте его в файлы проекта):

```sh
export TELEGRAM_BOT_TOKEN='<token-from-BotFather>'
```

Пользователь должен заранее открыть бота и нажать **Start**. Укажите его Telegram `chat_id` при создании пользователя в Core. Если `TELEGRAM_BOT_TOKEN` не задан, Telegram-канал отключается с предупреждением в логе.

**Email (опционально).** Почта отправляется через Gmail SMTP и Mailio. При первой сборке CMake скачает зафиксированную версию Mailio; для неё нужны CMake 3.30+, Boost 1.81+ с компонентами `date_time` и `regex`, а также OpenSSL. Для установки системных зависимостей на Ubuntu:

```sh
sudo apt update
sudo apt install libboost-date-time-dev libboost-regex-dev libssl-dev ca-certificates
```

Включите двухэтапную аутентификацию Google и создайте **пароль приложения** для почты. Обычный пароль Google использовать нельзя. Скопируйте пример и заполните `.env` новыми секретами (файл игнорируется Git):

```sh
cp .env.example .env
```

Отредактируйте `.env`: задайте `SMTP_ENABLED=true`, `SMTP_USERNAME`, `SMTP_PASSWORD` (пароль приложения без пробелов), `SMTP_FROM_EMAIL` (для Gmail укажите тот же адрес, что в `SMTP_USERNAME`); Gmail по умолчанию использует `smtp.gmail.com:587` и STARTTLS. Не вставляйте секреты в Git или HTTP-запросы. Запустите сервис, загрузив переменные окружения:

```sh
set -a
source .env
set +a
./build/bin/notification_service
```

Для отправки одного тестового письма из второго терминала используйте:

```sh
./scripts/test-email.sh получатель@example.com
```

Либо вызовите `POST http://127.0.0.1:8082/notifications/email/test` с JSON `{"email":"получатель@example.com"}`. Успех возвращает `"email":"sent"`, а ошибка SMTP — HTTP 502 с диагностическим сообщением. Проверка не гарантирует доставку во входящие: Gmail может поместить письмо в спам. Сервис слушает только локальный интерфейс `127.0.0.1:8082`.

Температурное уведомление сейчас формируется для существующего порога Core и имеет уровень `WARNING`. Повторные превышения подавляются по `device_id`, пока Core не получит нормальное показание. Состояние хранится в памяти Notification Service и сбрасывается при его перезапуске; между перезапусками или несколькими экземплярами сервиса дедупликация не сохраняется. Обработчик `POST /notifications/email` принимает общие поля события и уровни `INFO`, `WARNING`, `CRITICAL` для дальнейшего расширения.

### 4. Запустить HTTP Adapter

Оставьте Core запущенным. Откройте отдельный терминал в корне проекта:

```sh
./build/bin/http_adapter
```

Ожидаемый порт по умолчанию: `8081`. В конфигурации Adapter задан адрес Core `http://127.0.0.1:8080`.

Удобно держать PostgreSQL, Core, Notification Service и Adapter в отдельных терминалах. Для уведомлений должны быть запущены Core и Notification Service.

### Конфигурационные файлы

Исходные настройки хранятся здесь:

- `iot_core/config/HTTPConfig.json` — адрес и порт Core;
- `iot_core/config/PostgreSQLConfig.json` — host, порт, база и учётные данные PostgreSQL;
- `http_adapter/src/config/AdapterConfig.json` — адрес/порт Adapter и URL Core;
- `.env.example` — пример переменных SMTP; реальные секреты хранятся только в локальном `.env` или окружении;
- `SMTP_ENABLED` — включает SMTP (по умолчанию `false`);
- `SMTP_HOST`, `SMTP_PORT` — SMTP-сервер и порт (по умолчанию `smtp.gmail.com:587`);
- `SMTP_SECURITY` — `starttls` (по умолчанию) или implicit TLS `tls`; открытый SMTP запрещён;
- `SMTP_USERNAME`, `SMTP_PASSWORD` — логин и пароль приложения;
- `SMTP_FROM_EMAIL`, `SMTP_SENDER_NAME` — адрес и имя отправителя;
- `SMTP_TIMEOUT_SECONDS` — сетевой таймаут (по умолчанию 20 секунд);
- `PLATFORM_URL` — необязательная ссылка на платформу в письме; кнопка скрыта, если URL не задан.
- В `iot_core/config/HTTPConfig.json` в `custom_config` задаются `temperature_threshold` и `notification_service_url`.

CMake копирует HTTP-конфиги и email-шаблоны в build-каталог. После изменения исходных шаблонов повторите конфигурацию/сборку.

## Общий сценарий использования

Обычный порядок действий:

1. Создать пользователя через Core и получить `user_id`.
2. Создать устройство для пользователя и получить `device_id`.
3. Записывать показания напрямую в Core или через Adapter.
4. Читать показания.
5. При необходимости удалить телеметрию, устройство или пользователя.

Все данные запросов в этом руководстве передаются **в JSON-теле** (`-d` у `curl`). Ни один идентификатор или значение не помещается в query-параметры URL. Исключение — `GET /ping`: ему тело не требуется, и он не принимает входные данные.

Для JSON-запросов используйте заголовок:

```text
Content-Type: application/json
```

## HTTP API

### Сводная таблица маршрутов

#### IoT Core (`http://localhost:8080`)

| Метод | Путь | Тело JSON | Назначение |
|---|---|---|---|
| `GET` | `/ping` | нет | Проверка доступности |
| `POST` | `/users` | `email`, `password`, `name` | Создать пользователя |
| `DELETE` | `/users` | `user_id` | Удалить пользователя |
| `POST` | `/devices` | `owner_id`, `name`, `type` | Создать устройство |
| `DELETE` | `/devices` | `device_id` | Удалить устройство |
| `POST` | `/telemetry` | `device_id`, `temperature` | Добавить показание |
| `POST` | `/telemetry/query` | `device_id` | Получить до 10 последних показаний |
| `DELETE` | `/telemetry` | `device_id` | Удалить все показания устройства |

#### HTTP Adapter (`http://localhost:8081`)

| Метод | Путь | Тело JSON | Назначение |
|---|---|---|---|
| `POST` | `/ingest` | `device_id`, `temperature` | Переслать показание в Core |
| `POST` | `/telemetry/query` | `device_id` | Получить показания через Core |
| `DELETE` | `/telemetry` | `device_id` | Удалить показания через Core |

Adapter не реализует маршруты пользователей и устройств. Их нужно создавать и удалять напрямую через Core.

### Проверить Core — `GET /ping`

```sh
curl -i http://localhost:8080/ping
```

Тело ответа:

```text
pong
```

Маршрут зарегистрирован только в Core.

### Создать пользователя — `POST /users`

```sh
curl -i -X POST http://localhost:8080/users \
  -H 'Content-Type: application/json' \
  -d '{"email":"user@example.com","password":"secret","name":"Alex","telegram_chat_id":"123456789"}'
```

Поля:

| Поле | Тип | Значение |
|---|---|---|
| `email` | строка | Email; в БД должен быть уникальным |
| `password` | строка | Пароль пользователя |
| `name` | строка | Отображаемое имя |
| `telegram_chat_id` | строка или целое число | Необязательный ID чата с ботом; без него уведомления отправляться не будут |

Успешный ответ содержит созданный идентификатор:

```json
{"status":"ok","user_id":1}
```

При ошибке БД ответ имеет вид `{"status":"error","message":"..."}`. Например, повторный email не проходит ограничение уникальности.

### Удалить пользователя — `DELETE /users`

ID передаётся телом запроса, не URL:

```sh
curl -i -X DELETE http://localhost:8080/users \
  -H 'Content-Type: application/json' \
  -d '{"user_id":1}'
```

Успешный ответ:

```json
{"status":"ok","message":"User deleted"}
```

При удалении пользователя PostgreSQL каскадно удаляет все его устройства и телеметрию этих устройств.

### Создать устройство — `POST /devices`

```sh
curl -i -X POST http://localhost:8080/devices \
  -H 'Content-Type: application/json' \
  -d '{"owner_id":1,"name":"Living room sensor","type":"temperature"}'
```

Поля:

| Поле | Тип | Значение |
|---|---|---|
| `owner_id` | целое число | ID ранее созданного пользователя |
| `name` | строка | Название устройства |
| `type` | строка | Категория/тип устройства |

Успешный ответ:

```json
{"status":"ok","device_id":1}
```

`owner_id` должен ссылаться на существующую строку в `users`.

### Удалить устройство — `DELETE /devices`

```sh
curl -i -X DELETE http://localhost:8080/devices \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
```

Успешный ответ:

```json
{"status":"ok","message":"Device deleted"}
```

Показания этого устройства удаляются каскадно.

### Записать показание напрямую — `POST /telemetry` в Core

```sh
curl -i -X POST http://localhost:8080/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"temperature":23.5}'
```

Поля:

| Поле | Тип | Значение |
|---|---|---|
| `device_id` | целое число | ID существующего устройства |
| `temperature` | число | Показание температуры |

Успешный ответ:

```json
{"status":"ok"}
```

Каждый запрос вставляет **новую** запись. Время ставится базой автоматически. `device_id` должен ссылаться на существующее устройство.

Если температура выше `temperature_threshold` из `iot_core/config/HTTPConfig.json`, Core находит владельца устройства и его контакты (`telegram_chat_id`, `email`), затем вызывает Notification Service. Уведомление отправляется параллельно по всем настроенным каналам. Ошибка доставки логируется отдельно и не отменяет уже сохранённую телеметрию.

В ответе на сохранение телеметрии поле `notification_status` показывает итог:
- `not_required` — температура не выше порога;
- `sent` — все отправляемые каналы доставили;
- `sent_partial: ...` — часть каналов доставила, часть упала;
- `delivery_failed: ...` — все каналы упали;
- `no_contact_info` — у пользователя нет ни `telegram_chat_id`, ни `email`;
- `device_lookup_failed: ...` / `user_lookup_failed: ...` — ошибка поиска.

### Записать показание через Adapter — `POST /ingest`

```sh
curl -i -X POST http://localhost:8081/ingest \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"temperature":23.5}'
```

Тело совпадает с прямым запросом к Core. Успешный ответ Adapter:

```json
{"status":"ok","message":"Data forwarded to IoT Core"}
```

Adapter делает внутренний HTTP `POST /telemetry` в Core. Если Core недоступен или отвергает запрос, Adapter возвращает `status: error` и описание ошибки. Запись в любом случае создаётся в таблице `telemetry` Core, а не в отдельном хранилище Adapter.

### Получить телеметрию напрямую — `POST /telemetry/query` в Core

Для чтения используется `POST`, чтобы передавать `device_id` в JSON-теле: тела у `GET` не поддерживаются одинаково всеми HTTP-клиентами и промежуточными прокси.

```sh
curl -i -X POST http://localhost:8080/telemetry/query \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
```

Core возвращает JSON-массив максимум из 10 последних строк, отсортированных от новых к старым:

```json
[
  {
    "id": 15,
    "device_id": 1,
    "temperature": 23.5,
    "time": "2026-10-25 01:35:00"
  },
  {
    "id": 14,
    "device_id": 1,
    "temperature": 22.9,
    "time": "2026-10-25 01:30:00"
  }
]
```

Если записей нет, результатом будет пустой массив `[]`.

### Получить телеметрию через Adapter — `POST /telemetry/query`

```sh
curl -i -X POST http://localhost:8081/telemetry/query \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
```

Adapter отправит запрос в Core и обернёт результат:

```json
{
  "status": "ok",
  "data": [
    {
      "id": 15,
      "device_id": 1,
      "temperature": 23.5,
      "time": "2026-10-25 01:35:00"
    }
  ]
}
```

### Удалить телеметрию напрямую — `DELETE /telemetry` в Core

```sh
curl -i -X DELETE http://localhost:8080/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
```

Успешный ответ:

```json
{"status":"ok","message":"Telemetry deleted"}
```

Удаляются **все показания устройства**, а не одна запись.

### Удалить телеметрию через Adapter — `DELETE /telemetry`

```sh
curl -i -X DELETE http://localhost:8081/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
```

Adapter проксирует удаление в Core. Успешный ответ:

```json
{"status":"ok","message":"Telemetry deleted via adapter"}
```

### Сводка всех примеров запросов

Во всех примерах с ID запрос не содержит query-параметров; значения находятся в JSON:

```sh
# Проверка Core — у этого запроса нет входных данных
curl http://localhost:8080/ping

# Создание пользователя и устройства
curl -X POST http://localhost:8080/users -H 'Content-Type: application/json' \
  -d '{"email":"user@example.com","password":"secret","name":"Alex"}'
curl -X POST http://localhost:8080/devices -H 'Content-Type: application/json' \
  -d '{"owner_id":1,"name":"Sensor","type":"temperature"}'

# Запись: непосредственно в Core или через Adapter
curl -X POST http://localhost:8080/telemetry -H 'Content-Type: application/json' \
  -d '{"device_id":1,"temperature":23.5}'
curl -X POST http://localhost:8081/ingest -H 'Content-Type: application/json' \
  -d '{"device_id":1,"temperature":23.5}'

# Чтение: непосредственно из Core или через Adapter
curl -X POST http://localhost:8080/telemetry/query -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
curl -X POST http://localhost:8081/telemetry/query -H 'Content-Type: application/json' \
  -d '{"device_id":1}'

# Удаление телеметрии, устройства и пользователя
curl -X DELETE http://localhost:8080/telemetry -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
curl -X DELETE http://localhost:8080/devices -H 'Content-Type: application/json' \
  -d '{"device_id":1}'
curl -X DELETE http://localhost:8080/users -H 'Content-Type: application/json' \
  -d '{"user_id":1}'
```

Обратите внимание: удаление пользователя удалит его устройства и телеметрию, поэтому отдельный вызов удаления устройства после этого не нужен.

## Изменение и удаление данных

В HTTP API реализованы создание и удаление, но **не реализованы `PUT`/`PATCH` для изменения существующего пользователя или устройства**. Отправка нового показания не меняет старое — она создаёт ещё одну строку.

Изменить отдельное показание можно напрямую SQL-командой по `id` записи:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform \
  -c 'UPDATE telemetry SET temperature = 24.0 WHERE id = 15;'
```

Например, изменить название устройства:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform \
  -c "UPDATE devices SET name = 'Kitchen sensor' WHERE id = 1;"
```

## Структура базы данных

Инициализирующий скрипт `init.sql` задаёт три таблицы:

```text
users
  id          SERIAL, первичный ключ
  email       TEXT, уникальный и обязательный
  password    TEXT, обязательный
  name        TEXT
      │
      │ users.id ← devices.owner_id (ON DELETE CASCADE)
      ▼
devices
  id          SERIAL, первичный ключ
  owner_id    INTEGER, обязательная ссылка на users.id
  name        TEXT, обязательное название
  type        TEXT, обязательный тип
      │
      │ devices.id ← telemetry.device_id (ON DELETE CASCADE)
      ▼
telemetry
  id          SERIAL, первичный ключ
  device_id   INTEGER, обязательная ссылка на devices.id
  temperature REAL, обязательное показание
  time        TIMESTAMP, по умолчанию текущее время
```

Удаление пользователя автоматически удаляет зависимые устройства и их телеметрию. Удаление устройства автоматически удаляет его телеметрию.

## Работа с PostgreSQL вручную

Посмотреть статус контейнеров:

```sh
docker compose ps -a
```

Посмотреть таблицы:

```sh
docker compose exec postgres psql -U postgres -d iot_platform -c '\dt'
```

Посмотреть пользователей:

```sh
docker compose exec postgres psql -U postgres -d iot_platform \
  -c 'SELECT id, email, name FROM users ORDER BY id;'
```

Посмотреть устройства:

```sh
docker compose exec postgres psql -U postgres -d iot_platform \
  -c 'SELECT id, owner_id, name, type FROM devices ORDER BY id;'
```

Посмотреть новые показания:

```sh
docker compose exec postgres psql -U postgres -d iot_platform \
  -c 'SELECT id, device_id, temperature, time FROM telemetry ORDER BY id DESC;'
```

Посмотреть логи автоматической инициализации:

```sh
docker compose logs db-init
```

## Остановка и сброс базы

Остановить Compose-сервисы, оставив данные в volume:

```sh
docker compose down
```

После следующего `docker compose up -d` PostgreSQL запустится с сохранёнными данными, а `db-init` снова применит `init.sql`. Благодаря `IF NOT EXISTS` существующие таблицы не пересоздаются.

### Сбросить данные и заново создать чистую базу

**Осторожно: это безвозвратно удалит volume со всеми данными.**

```sh
docker compose down -v
docker compose up -d
```

После создания чистой базы автоматический `db-init` создаст таблицы из актуального `init.sql`.

### Повторно применить `init.sql` вручную

В обычном режиме это не нужно: `db-init` выполняет скрипт при запуске Compose. Если требуется вызвать его отдельно:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform \
  -v ON_ERROR_STOP=1 < init.sql
```

`init.sql` создаёт отсутствующие таблицы и добавляет отсутствующую колонку `telegram_chat_id` к уже существующей таблице `users`. Для других изменений схемы нужны отдельные SQL-миграции или сброс базы.

## Ограничения и безопасность

- Проект предназначен для учебного/локального использования; в API пока нет аутентификации и проверки прав доступа.
- Notification Service не использует внутренний токен и слушает только `127.0.0.1`. Если сервисы будут размещены на разных машинах или endpoint станет доступен из сети, добавьте аутентификацию и защищённый канал связи до публикации.
- Telegram-уведомления отправляются только если пользователь заранее начал диалог с ботом и у него задан `telegram_chat_id`.
- Отправка выполняется синхронным по смыслу HTTP-вызовом после записи телеметрии; устойчивой очереди и автоматических повторов пока нет.
- Пароль пользователя сохраняется в PostgreSQL как обычный текст. Не отправляйте реальные пароли и не используйте эту схему в production без безопасного хеширования.
- Настройки PostgreSQL в Compose (`postgres` / `postgres`) — локальные демонстрационные значения.
- Список пользователей и устройств через HTTP не выдаётся; для просмотра используйте PostgreSQL.
- Чтение телеметрии возвращает не более 10 последних записей.
- Телеметрия сейчас хранит только `temperature`; другие показатели не предусмотрены.
- API не предоставляет `PUT`/`PATCH` для изменения пользователя или устройства.
- Adapter предназначен только для телеметрии; создание и удаление пользователей/устройств выполняется через Core.
