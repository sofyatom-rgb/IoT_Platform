# IoT Platform

## Запуск после скачивания

Нужны CMake, C++-компилятор, Drogon и Docker Compose.

Из корня проекта соберите приложения:

```sh
mkdir -p build
cd build
cmake ..
make
cd ..
```

Запустите PostgreSQL и создайте таблицу телеметрии:

```sh
docker compose up -d
docker compose exec -T postgres psql -U postgres -d iot_platform \
  -c "CREATE TABLE IF NOT EXISTS telemetry (id BIGSERIAL PRIMARY KEY, device_id INTEGER NOT NULL, temperature DOUBLE PRECISION NOT NULL);"
```

Запустите бинарники **в двух отдельных терминалах из корня проекта**:

```sh
./build/bin/iot_core
```

```sh
./build/bin/http_adapter
```

То же можно сделать в VS Code: открыть корень проекта, выполнить CMake
Configure и Build, запустить PostgreSQL командой `docker compose up -d`, а
затем открыть два терминала и запустить бинарники.

## Проверка и данные

Проверить адаптер:

```sh
curl http://localhost:8081/ping
```

Отправить показание через HTTP Adapter (его адрес — порт `8081`):

```sh
curl 'http://localhost:8081/set_telemetry?device_id=1&temperature=23.5'
```

Или отправить JSON прямо в IoT Core (порт `8080`):

```sh
curl -X POST http://localhost:8080/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"temperature":23.5}'
```

Посмотреть таблицы и сохранённые показания:

```sh
docker compose exec postgres psql -U postgres -d iot_platform -c '\dt'
docker compose exec postgres psql -U postgres -d iot_platform \
  -c 'SELECT * FROM telemetry ORDER BY id DESC;'
```

Остановить PostgreSQL:

```sh
docker compose down
```

Настройки Core находятся в `iot_core/src/telemetry/config/`:
`HTTPConfig.json` задаёт HTTP-порт, `PostgreSQLConfig.json` — подключение к базе.
