# IoT Platform

IoT Platform — учебный backend для регистрации пользователей и устройств, приёма температурных показаний, чтения телеметрии и отправки уведомлений в Telegram. Сервисы написаны на C++ с Drogon, данные хранятся в PostgreSQL.

В проекте есть четыре компонента с HTTP API:

1. **IoT Core** — основной сервис и единственный компонент, который работает с PostgreSQL. Адрес по умолчанию: `http://localhost:8080`.
2. **HTTP Adapter** — дополнительный HTTP-вход для интеграций и IoT-клиентов. Адрес по умолчанию: `http://localhost:8081`. Он принимает телеметрию и проксирует запросы к IoT Core.
3. **Notification Service** — внутренний HTTP-сервис, принимающий события от Core и отправляющий сообщения через Telegram Bot API. Адрес по умолчанию: `http://localhost:8082`.
4. **Rule Engine** — сервис управления температурными правилами и отправки уведомлений при их нарушении. Адрес по умолчанию: `http://localhost:8083`.

## Содержание

- [Как устроен проект](#как-устроен-проект)
- [Поток данных](#поток-данных)
- [Требования](#требования)
- [Сборка](#сборка)
- [Unit-тесты](#unit-тесты)
- [Логирование](#логирование)
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
│       │   │   └── NotificationServiceClient.cpp # HTTP-клиент для Notification Service
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
├── rule_engine/                     # Сервис температурных правил
│   ├── CMakeLists.txt
│   ├── config/config.json           # Порт и адрес Notification Service
│   └── src/
│       ├── controllers/             # HTTP-маршруты правил
│       ├── services/                # Проверка показаний по правилам
│       ├── repositories/            # SQL для rules и device_to_rule
│       └── clients/                 # Вызовы Notification Service
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
│       └── clients/                  # Отправляет сообщения в Telegram Bot API
│
└── build-app/                        # Генерируется CMake; здесь находятся бинарники
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
- Core вызывает Notification Service через `NotificationServiceClient`; его контроллер вызывает сервис уведомлений, а тот — Telegram Bot API через `TelegramBotClient`.
- `NotificationService` зависит только от `ITelegramBotClient`. Реальный `TelegramBotClient` создаётся в `main.cpp`, а в тестах зависимость подменяется моками FakeIt.

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
                                   Notification Service → TelegramBotClient
                                                      │ HTTPS
                                                      ▼
                                              Telegram Bot API
```

### Когда обращаться напрямую, а когда через Adapter

**Напрямую в Core** удобно для локальной разработки, тестирования API и внутренних клиентов, которым доступен порт Core. Core предоставляет операции пользователей, устройств и телеметрии.

**Через Adapter** удобно направлять телеметрию от внешних клиентов через отдельную точку входа. Adapter отделяет внешний ingest API от внутреннего Core и может служить местом для будущей интеграционной логики. Сейчас он пересылает телеметрию без дополнительной обработки; пользователей и устройства он не обслуживает.

Оба пути записи сохраняют данные в одну и ту же базу: Adapter не имеет собственного подключения к PostgreSQL. Если нужен только один процесс при локальной проверке, запросы телеметрии можно направлять напрямую в Core; для работы Adapter должны быть запущены оба сервиса.

Если превышен температурный порог, Core запрашивает владельца устройства и его Telegram chat ID из PostgreSQL, затем отправляет событие в отдельный Notification Service.

## Требования

- Linux/macOS или другая среда, где доступны инструменты проекта;
- CMake версии 3.16 или новее;
- C++-компилятор с поддержкой C++26;
- установленный Drogon и его зависимости — для сборки приложений, тестов контроллеров и реального HTTP-транспорта; базовые unit-тесты без этих компонентов собираются без Drogon;
- OpenSSL и JsonCpp development-пакеты — для приложения и дополнительных codec-тестов (`BUILD_CODEC_TESTING=ON`);
- PostgreSQL — для запуска приложения и отдельных integration-тестов репозиториев;
- переменная окружения `PASSWORD_PEPPER` для запуска IoT Core;
- Docker и Docker Compose plugin;
- `curl` для примеров ниже (необязательно, можно использовать любой HTTP-клиент).

Проект вызывает `find_package(Drogon CONFIG REQUIRED)`, поэтому установленный пакет Drogon должен быть виден CMake.

## Сборка приложения и тестов

Команды выполняются из корня репозитория. Приложение и тесты собираются в **разных
каталогах**: `build-app` и `build-tests`. Не нужно включать `BUILD_APPLICATION=ON`,
чтобы собрать тесты.

### 1. Собрать только приложение

```sh
cmake -S . -B build-app \
  -DBUILD_APPLICATION=ON \
  -DBUILD_TESTING=OFF
cmake --build build-app -j"$(nproc)"
```

Будут собраны четыре приложения: `iot_core`, `http_adapter`,
`notification_service` и `rule_engine`. Бинарники и конфигурационные файлы
появятся в `build-app/bin/`. Тестовые цели в этой конфигурации не создаются.

Повторная сборка после изменения исходников:

```sh
cmake --build build-app -j"$(nproc)"
```

Полная пересборка приложения с нуля:

```sh
rm -rf build-app
cmake -S . -B build-app -DBUILD_APPLICATION=ON -DBUILD_TESTING=OFF
cmake --build build-app -j"$(nproc)"
```

### 2. Собрать тесты отдельно от приложения

Для основного набора unit- и component-тестов без Drogon и PostgreSQL:

```sh
cmake -S . -B build-tests \
  -DBUILD_APPLICATION=OFF \
  -DBUILD_TESTING=ON \
  -DBUILD_CODEC_TESTING=ON \
  -DBUILD_CONTROLLER_TESTING=OFF \
  -DBUILD_POSTGRES_INTEGRATION_TESTING=OFF \
  -DBUILD_TRANSPORT_INTEGRATION_TESTING=OFF
cmake --build build-tests -j"$(nproc)"
```

В этой конфигурации запускаются **57 сценариев**. Флаг `BUILD_CODEC_TESTING`
включает `codec_unit_tests` — ещё 4 сценария кодирования/декодирования, которым
нужны OpenSSL и JsonCpp. Четыре простых теста Base64/CRC32 (`codec_primitive_tests`)
собираются независимо от этого флага. Если development-пакет JsonCpp не установлен,
можно временно указать `-DBUILD_CODEC_TESTING=OFF`: тогда останется 53 сценария.
В Ubuntu/Debian необходимые пакеты устанавливаются командой:

```sh
sudo apt install libssl-dev libjsoncpp-dev
```

Запустить тесты:

```sh
ctest --test-dir build-tests --output-on-failure
```

### 3. Собрать расширенный набор тестов, включая контроллеры и HTTP-транспорт

Эта конфигурация всё ещё **не собирает приложения**. Она собирает только тестовые
исполняемые файлы. Для контроллеров и реального Drogon HTTP-транспорта потребуется
установленный Drogon; для codec-тестов — OpenSSL и JsonCpp. PostgreSQL не нужен.

```sh
cmake -S . -B build-tests-full \
  -DBUILD_APPLICATION=OFF \
  -DBUILD_TESTING=ON \
  -DBUILD_CODEC_TESTING=ON \
  -DBUILD_CONTROLLER_TESTING=ON \
  -DBUILD_TRANSPORT_INTEGRATION_TESTING=ON \
  -DBUILD_POSTGRES_INTEGRATION_TESTING=OFF
cmake --build build-tests-full -j"$(nproc)"
ctest --test-dir build-tests-full --output-on-failure
```

Ожидается **105 сценариев**. Контроллеры тестируются напрямую, а их сервисные
зависимости подменяются FakeIt. Восемь `drogon_transport_integration_tests` проверяют
реальный HTTP-транспорт через локальный TCP-сервер. Ни один бинарник приложения
для этого собирать не требуется.

### 4. Запустить полный набор из 113 сценариев, включая PostgreSQL

Ещё восемь сценариев проверяют реальные SQL-репозитории. Это интеграционные тесты:
они должны обращаться к PostgreSQL, иначе проверка SQL, ограничений и внешних ключей
теряет смысл.

Сначала запустите PostgreSQL:

```sh
docker compose up -d postgres
```

Создайте отдельную тестовую базу (эту команду нужно выполнить один раз):

```sh
docker compose exec postgres psql -U postgres -d postgres \
  -c "CREATE DATABASE iot_platform_test;"
```

Примените схему:

```sh
docker compose exec -T postgres psql -U postgres -d iot_platform_test \
  -v ON_ERROR_STOP=1 < init.sql
```

Соберите тесты. Обратите внимание: `BUILD_APPLICATION` остаётся `OFF` — собираются
тестовые цели, а не четыре микросервиса.

```sh
cmake -S . -B build-tests-full \
  -DBUILD_APPLICATION=OFF \
  -DBUILD_TESTING=ON \
  -DBUILD_CODEC_TESTING=ON \
  -DBUILD_CONTROLLER_TESTING=ON \
  -DBUILD_TRANSPORT_INTEGRATION_TESTING=ON \
  -DBUILD_POSTGRES_INTEGRATION_TESTING=ON
cmake --build build-tests-full -j"$(nproc)"
```

Запустите все тесты с переменной подключения к тестовой БД:

```sh
IOT_TEST_DATABASE_URL='host=127.0.0.1 port=5433 dbname=iot_platform_test user=postgres password=postgres' \
  ctest --test-dir build-tests-full --output-on-failure
```

При успешном выполнении должны пройти все **113 сценариев**.

### 5. Пересборка и очистка тестов

После изменения исходников тестов достаточно повторить сборку и запуск:

```sh
cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests --output-on-failure
```

Чтобы полностью пересоздать тестовую конфигурацию:

```sh
rm -rf build-tests
cmake -S . -B build-tests \
  -DBUILD_APPLICATION=OFF \
  -DBUILD_TESTING=ON \
  -DBUILD_CODEC_TESTING=ON \
  -DBUILD_CONTROLLER_TESTING=OFF \
  -DBUILD_POSTGRES_INTEGRATION_TESTING=OFF \
  -DBUILD_TRANSPORT_INTEGRATION_TESTING=OFF
cmake --build build-tests -j"$(nproc)"
ctest --test-dir build-tests --output-on-failure
```

### Запуск отдельных наборов

CTest регистрирует не каждый `TEST_CASE`, а исполняемые файлы. Поэтому строка
`100% tests passed, 0 tests failed out of N` считает наборы, а не сценарии внутри них.
Внутри каждого набора `TestFramework` печатает отдельный `[PASS]` и итог `Test summary`.
Успешные сценарии окрашиваются зелёным, ошибки — красным. Чтобы видеть вывод каждого
сценария, используй `ctest --verbose`: обычный `ctest --output-on-failure` скрывает вывод
успешных наборов. Для CTest цвета принудительно включаются через `FORCE_COLOR=1`.

Полезные команды:

```sh
# Показать зарегистрированные CTest-наборы
ctest --test-dir build-tests-full -N

# Показать каждый сценарий: [PASS] будет зелёным, [FAIL] — красным
ctest --test-dir build-tests-full --verbose

# Запустить один набор напрямую — цвет определяется по терминалу
./build-tests-full/tests/service_unit_tests

# Принудительно включить цвета, например при перенаправлении вывода
FORCE_COLOR=1 ./build-tests-full/tests/service_unit_tests

# Отключить цвета
NO_COLOR=1 ctest --test-dir build-tests-full --verbose

# Повторить только клиентские тесты
ctest --test-dir build-tests-full -R client_unit_tests --output-on-failure

# Повторить только тесты контроллеров
ctest --test-dir build-tests-full -R controller_unit_tests --output-on-failure

# Собрать только один исполняемый файл
cmake --build build-tests-full --target client_unit_tests -j"$(nproc)"

# Запустить его напрямую — он выведет каждый TEST_CASE
./build-tests-full/tests/client_unit_tests
```

### Какие сценарии входят в полный набор

| Набор | Сценарии | Что проверяется |
|---|---:|---|
| `service_unit_tests` | 22 | Сервисы пользователей, устройств, телеметрии, адаптера и уведомлений |
| `client_unit_tests` | 14 | IoT Core, Notification Service и Telegram HTTP-клиенты с FakeIt-транспортом |
| `controller_unit_tests` | 40 | HTTP-обработчики и ответы контроллеров с FakeIt-моками сервисов |
| `controller_validation_tests` | 9 | Проверка ID и входных полей без Drogon |
| `adapter_pipeline_tests` | 2 | Настоящий AdapterService и IoTClient с FakeIt HTTP-транспортом |
| `telemetry_notification_pipeline_tests` | 2 | Цепочка сохранения телеметрии и отправки уведомления с FakeIt-зависимостями |
| `codec_primitive_tests` | 4 | Base64 и CRC32 без сторонних библиотек |
| `codec_unit_tests` | 4 | Шифрование, дешифрование, IV, checksum и некорректные конверты |
| `drogon_transport_integration_tests` | 8 | Настоящие HTTP-запросы через Drogon и локальный TCP-сервер |
| `repository_integration_tests` | 8 | Настоящие запросы PostgreSQL, CRUD, ограничения и каскадное удаление |
| **Всего** | **113** | |

### Как применяется FakeIt

FakeIt уже находится в `tests/third_party/fakeit` — отдельно устанавливать его не нужно.
В unit- и component-тестах нет самописных классов, которые вручную реализуют
`IUsersRepository`, `IDevicesRepository`, `ITelemetryRepository`, сервисы или HTTP-порты.
Для подмены используются объекты `fakeit::Mock<Интерфейс>`:

```cpp
auto repositoryMock = std::make_shared<fakeit::Mock<IUsersRepository>>();
auto repository = test::sharedMock(repositoryMock);

fakeit::When(Method(*repositoryMock, save))
    .Do([](const std::string &, const std::string &, const std::string &,
           const std::string &, std::function<void(int)> success,
           std::function<void(const std::string &)>) {
      success(42);
    });

// После вызова сервиса проверяем, что репозиторий вызван один раз.
fakeit::Verify(Method(*repositoryMock, save)).Once();
```

`test::sharedMock` из `tests/TestSupport.h` только передаёт FakeIt-мок через
`shared_ptr`, сохраняя время его жизни. Для HTTP-клиентов есть `HttpTransportMock`:
это небольшая обёртка **над FakeIt**, которая запоминает запросы и возвращает
настраиваемый ответ. Сам мок `IHttpTransport` создаётся внутри неё через
`fakeit::Mock<IHttpTransport>`; отдельной ручной реализации транспорта нет.

Настоящие PostgreSQL и Drogon transport tests остаются интеграционными: в них проверяется
поведение с реальной БД и реальным сокетом. Подменять эти части FakeIt означало бы
потерять смысл таких проверок.


## Логирование

Во всех четырёх сервисах используется встроенный логгер Drogon (`LOG_INFO`,
`LOG_WARN`, `LOG_ERROR`, `LOG_FATAL`). Общая настройка находится в
`common/logging/DrogonLogging.h` и подключается при запуске каждого приложения.

### Что попадает в журнал

| Событие | Уровень | Пример |
|---|---|---|
| Запуск сервиса и подключение инфраструктуры | `INFO` | `[iot_core] Starting IoT Core` |
| Входящий HTTP-запрос | `INFO` | `HTTP request: POST /telemetry` |
| Успешный HTTP-ответ | `INFO` | `HTTP response: POST /telemetry -> 200` |
| Клиентская ошибка HTTP | `WARN` | `HTTP response: POST /users -> 400` |
| Ошибка сервера HTTP | `ERROR` | `HTTP response: POST /telemetry -> 500` |
| Начало SQL-операции и её успешное завершение | `INFO` | `[iot_core][db] telemetry.save: started` |
| Ошибка SQL | `ERROR` | `[iot_core][db] telemetry.save failed` |
| Исходящий HTTP-запрос и ответ | `INFO` | `[http] -> POST http://127.0.0.1:8082/notifications/temperature` |
| Ошибка сети или неуспешный исходящий HTTP-ответ | `ERROR` | `[http] <- POST ... failed; status=0 network_error` |

Логируются входящие запросы всех маршрутов каждого сервиса, обращения к PostgreSQL
в `iot_core` и `rule_engine`, исходящие HTTP-вызовы из Adapter/Core/Notification
Service, а также вызов Notification Service из Rule Engine.

### Где находятся файлы

При старте приложение создаёт каталог `logs` рядом со своим исполняемым файлом.
При стандартной сборке это `build-app/bin/logs/`:

```text
build-app/bin/logs/
├── iot_core.log
├── http_adapter.log
├── notification_service.log
└── rule_engine.log
```

Каждый сервис пишет в отдельный файл. Размер файла ограничен 10 MiB, Drogon
хранит до пяти файлов ротации. Логгер настраивается на уровень `INFO`, поэтому
в журнал попадают сообщения `INFO`, `WARN`, `ERROR` и `FATAL`.

### Как посмотреть логи

Открой отдельный терминал и следи за логом нужного сервиса:

```sh
# IoT Core
tail -f build-app/bin/logs/iot_core.log

# Adapter
tail -f build-app/bin/logs/http_adapter.log

# Notification Service
tail -f build-app/bin/logs/notification_service.log

# Rule Engine
tail -f build-app/bin/logs/rule_engine.log
```

Фильтрация ошибок:

```sh
grep -E '\[ERROR\]|\[FATAL\]' build-app/bin/logs/*.log
```

### Как продемонстрировать работу

1. Собери приложение по инструкции из раздела [Сборка](#сборка), запусти PostgreSQL
   вместе с одноразовым сервисом инициализации схемы:

   ```sh
   docker compose up -d
   export PASSWORD_PEPPER='local-development-pepper'
   ./build-app/bin/iot_core
   ```

2. В другом терминале отправь запрос проверки доступности:

   ```sh
   curl -i http://127.0.0.1:8080/ping
   ```

   В `iot_core.log` появятся входящий `GET /ping` и ответ `200` (`INFO`).

3. Отправь заведомо некорректный запрос:

   ```sh
   curl -i -X POST http://127.0.0.1:8080/telemetry \
     -H 'Content-Type: application/json' \
     -d '{"device_id":-1,"temperature":22.5}'
   ```

   Контроллер ответит `400`; журнал покажет запрос и ответ с уровнем `WARN`.

4. Создай пользователя и устройство, чтобы увидеть реальные SQL-операции. Укажи
   свой тестовый Telegram chat ID, если планируешь проверять уведомления:

   ```sh
   curl -i -X POST http://127.0.0.1:8080/users \
     -H 'Content-Type: application/json' \
     -d '{"email":"demo@example.com","password":"demo-password","name":"Demo","telegram_chat_id":"<your-chat-id>"}'

   curl -i -X POST http://127.0.0.1:8080/devices \
     -H 'Content-Type: application/json' \
     -d '{"owner_id":<user-id-from-response>,"name":"Demo sensor","type":"temperature"}'

   curl -i -X POST http://127.0.0.1:8080/telemetry \
     -H 'Content-Type: application/json' \
     -d '{"device_id":<device-id-from-response>,"temperature":22.5}'
   ```

   В `iot_core.log` появятся `users.save`, `devices.save` и `telemetry.save` с
   записями `started` и `completed` (`INFO`). Замени заполнители реальными ID.

5. Запусти остальные сервисы в отдельных терминалах:

   ```sh
   # Терминал 2
   ./build-app/bin/http_adapter

   # Терминал 3
   TELEGRAM_BOT_TOKEN='<token-from-BotFather>' ./build-app/bin/notification_service

   # Терминал 4
   ./build-app/bin/rule_engine
   ```

   Проверь входящие запросы каждого сервиса:

   ```sh
   # Adapter: некорректный JSON даст 400 (WARN)
   curl -i -X POST http://127.0.0.1:8081/ingest \
     -H 'Content-Type: application/json' -d '{'

   # Notification Service: пустое тело даст 400 (WARN)
   curl -i -X POST http://127.0.0.1:8082/notifications/temperature \
     -H 'Content-Type: application/json' -d '{}'

   # Rule Engine: ping даст 200 (INFO)
   curl -i http://127.0.0.1:8083/ping
   ```

6. Чтобы увидеть исходящие HTTP-запросы и Telegram-уведомление, отправь в Core
   температуру выше `temperature_threshold` из `iot_core/config/HTTPConfig.json`:

   ```sh
   curl -i -X POST http://127.0.0.1:8080/telemetry \
     -H 'Content-Type: application/json' \
     -d '{"device_id":<device-id-from-response>,"temperature":35}'
   ```

   В `iot_core.log` появится запрос к Notification Service, а в
   `notification_service.log` — запрос к Telegram Bot API. Для успешной доставки
   пользователь должен заранее открыть бота и нажать **Start**.

   Для Rule Engine создай правило и отправь температуру выше его порога:

   ```sh
   curl -i -X POST http://127.0.0.1:8083/rules \
     -H 'Content-Type: application/json' \
     -d '{"device_id":<device-id-from-response>,"operator":">","value":30}'

   curl -i -X POST http://127.0.0.1:8083/telemetry \
     -H 'Content-Type: application/json' \
     -d '{"device_id":<device-id-from-response>,"temperature":35}'
   ```

   В `rule_engine.log` будут видны SQL-операции, проверка правила и HTTP-вызов
   Notification Service.

7. Чтобы проверить `ERROR` PostgreSQL, временно останови БД и повтори запрос
   записи телеметрии:

   ```sh
   docker compose stop postgres
   curl -i -X POST http://127.0.0.1:8080/telemetry \
     -H 'Content-Type: application/json' \
     -d '{"device_id":1,"temperature":22.5}'
   docker compose start postgres
   ```

   В `iot_core.log` появится `[iot_core][db] telemetry.save failed`, а HTTP-ответ
   будет `500`. Подожди, пока PostgreSQL снова станет healthy, прежде чем
   продолжать работу.

### Безопасность логов

Логи намеренно не содержат HTTP-тела, query string и заголовки. Они также не
содержат пароли, хеши паролей, Telegram chat ID или содержимое сообщений. Это важно:
тела запросов регистрации и уведомлений содержат чувствительные данные. Telegram Bot
API передаёт токен в URL; общий HTTP-транспорт заменяет его в логах на `<redacted>`.
В PostgreSQL-журнал попадают имя операции и факт ошибки, но не значения параметров
`$1`, `$2` и т. д. Текст ошибки драйвера намеренно не записывается, поскольку он
может содержать значения полей из диагностического сообщения PostgreSQL.

Для локальной демонстрации используются тестовые данные. Не отправляй реальные
пароли и токены в публичные логи и не коммить секреты в репозиторий.

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
./build-app/bin/iot_core
```

Ожидаемый порт по умолчанию: `8080`.

### 3. Запустить Notification Service

Создайте бота через BotFather и задайте токен бота в окружении. Не добавляйте его в файлы проекта:

```sh
export TELEGRAM_BOT_TOKEN='<token-from-BotFather>'
```

Пользователь должен заранее открыть бота и нажать **Start**. Укажите его Telegram `chat_id` при создании пользователя в Core (см. HTTP API ниже).

В отдельном терминале из корня проекта запустите:

```sh
./build-app/bin/notification_service
```

Сервис слушает только локальный интерфейс `127.0.0.1:8082`; поэтому этот вариант рассчитан на запуск Core и Notification Service на одном компьютере. Токен бота нужен только Notification Service.

### 4. Запустить HTTP Adapter

Оставьте Core запущенным. Откройте отдельный терминал в корне проекта:

```sh
./build-app/bin/http_adapter
```

Ожидаемый порт по умолчанию: `8081`. В конфигурации Adapter задан адрес Core `http://127.0.0.1:8080`.

Удобно держать PostgreSQL, Core, Notification Service и Adapter в отдельных терминалах. Для уведомлений должны быть запущены Core и Notification Service.

### Конфигурационные файлы

Исходные настройки хранятся здесь:

- `iot_core/config/HTTPConfig.json` — адрес и порт Core;
- `iot_core/config/PostgreSQLConfig.json` — host, порт, база и учётные данные PostgreSQL;
- `http_adapter/src/config/AdapterConfig.json` — адрес/порт Adapter и URL Core;
- `notification_service/config/HTTPConfig.json` — адрес и порт Notification Service.
- В `iot_core/config/HTTPConfig.json` в `custom_config` задаются `temperature_threshold` и `notification_service_url`.

CMake копирует их в `build-app/bin`. После изменения конфигов повторите конфигурацию/сборку или скопируйте файлы в `build-app/bin`, чтобы приложение получило обновлённые настройки.

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

Если температура выше `temperature_threshold` из `iot_core/config/HTTPConfig.json`, Core находит владельца устройства и его `telegram_chat_id`, затем вызывает Notification Service. Ошибка доставки логируется отдельно и не отменяет уже сохранённую телеметрию.

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
  id            SERIAL, первичный ключ
  email         TEXT, уникальный и обязательный
  password_hash TEXT, хеш пароля
  salt          TEXT, соль для хеширования
  name          TEXT
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
- Пароль перед сохранением хешируется через PBKDF2-HMAC-SHA256 с уникальной солью и секретным `PASSWORD_PEPPER`. Для развёртывания задайте сильный pepper через переменную окружения и не храните его в репозитории.
- Настройки PostgreSQL в Compose (`postgres` / `postgres`) — локальные демонстрационные значения.
- Список пользователей и устройств через HTTP не выдаётся; для просмотра используйте PostgreSQL.
- Чтение телеметрии возвращает не более 10 последних записей.
- Телеметрия сейчас хранит только `temperature`; другие показатели не предусмотрены.
- API не предоставляет `PUT`/`PATCH` для изменения пользователя или устройства.
- Adapter предназначен только для телеметрии; создание и удаление пользователей/устройств выполняется через Core.
