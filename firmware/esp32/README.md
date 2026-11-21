# ESP32 DS18B20 → IoT Platform HTTP Adapter

Прошивка читает DS18B20 и отправляет показания на REST-эндпоинт адаптера
`POST /devices/{id}/telemetry` обычным JSON без шифрования:

```json
{"temperature":23.75}
```

`device_id` задаётся в `include/secrets.h` и передаётся в URL, поэтому в теле
остаётся только температура.

## Подключение

| DS18B20 | ESP32 DevKit |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO 4 |

Между DATA и 3.3V обязателен резистор 4.7 кОм.

## Файлы

```text
firmware/esp32/
├── platformio.ini
├── include/
│   ├── secrets.example.h   # шаблон, коммитится
│   └── secrets.h           # локальные значения, исключён из Git
├── src/
│   └── main.cpp            # Wi-Fi, DS18B20, HTTP
├── test/test_codec.cpp     # тесты старого кодека на компьютере
└── tools/run_native_tests.sh
```

Кодек остался в репозитории как reusable-компонент, но прошивка его больше не
использует.

## Локальная конфигурация

`include/secrets.h` уже создан локально и содержит Wi-Fi, `device_id` и адрес
адаптера. Файл исключён из Git:

```sh
git check-ignore -v firmware/esp32/include/secrets.h
```

Шаблон для другой машины — `include/secrets.example.h`. `ADAPTER_URL` должен
указывать на REST-путь устройства, например:

```c
#define DEVICE_ID 1
#define ADAPTER_URL "http://192.168.0.197:8081/devices/1/telemetry"
```

Wi-Fi пароль не выводится в Serial. Лог содержит IP, шлюз и маску — так видно,
в какой сети оказалась плата.

## Поведение

- первое измерение выполняется сразу после подключения к Wi-Fi;
- затем замер раз в минуту;
- неудачный POST повторяется в следующем цикле с тем же payload;
- после двух неудач выполняется принудительный reconnect Wi-Fi;
- в Serial выводятся температура, payload, HTTP-статус и сетевые события.

## Сборка и прошивка

PlatformIO установлен в `/home/kleshch/.venvs/platformio`.

```sh
cd /home/kleshch/Projects/IoT_Platform/firmware/esp32

# сборка
/home/kleshch/.venvs/platformio/bin/pio run -e esp32dev

# прошивка в /dev/ttyUSB0
/home/kleshch/.venvs/platformio/bin/pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0

# Serial-монитор
/home/kleshch/.venvs/platformio/bin/pio device monitor -b 115200 --port /dev/ttyUSB0
```

Пользователь должен быть в группе `dialout`:

```sh
groups | grep dialout
```

## Коды HTTP

| Код | Значение |
|---|---|
| `201` | Adapter принял и передал в Core |
| `400` | ошибка валидации (например, некорректная температура) |
| `502` | ошибка downstream; **запись могла уже сохраниться** в PostgreSQL |
| `< 0` | сетевая ошибка или таймаут |

При `502` прошивка не повторяет POST. Проверьте историю через
`GET /devices/1/telemetry`, чтобы не создать дубликат.

## Ограничения

- аутентификации устройства нет;
- HTTP без TLS;
- нет идемпотентности: повторная отправка создаёт новую строку;
- IP Adapter `192.168.0.197` может измениться по DHCP.

## Диагностика

- `400 Invalid JSON in Adapter` — отсутствует `Content-Type: application/json`
  (в прошивке он задан);
- `400 Expected a positive device ID in the URI...` — проверьте `ADAPTER_URL`;
- `502` — не запущены Core/Rule Engine/Notification Service;
- `[wifi] connection timeout` — сеть 2.4 ГГц, неверный пароль или слабый сигнал;
- `[sensor] no DS18B20 found` — проверить GPIO 4, питание и резистор 4.7 кОм.
