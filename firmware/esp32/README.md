# ESP32 DS18B20 → IoT Platform HTTP Adapter

Прошивка читает DS18B20, шифрует показание тем же кодеком, что и HTTP Adapter,
и отправляет зашифрованный конверт на REST-эндпоинт адаптера
`POST /devices/{id}/telemetry`:

```json
{"checksum":"...","ciphertext":"..."}
```

Внутри конверта находится `{"temperature":23.75}`. `device_id` задаётся в
`include/secrets.h` и передаётся в URL, поэтому в открытом виде он не шлётся.

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
│   ├── main.cpp            # Wi-Fi, DS18B20, кодек, HTTP
│   ├── codec.cpp           # AES-256-CBC, PKCS#7, CRC32, Base64
│   └── codec.h
├── test/test_codec.cpp     # тесты кодека на компьютере
└── tools/run_native_tests.sh
```

## Локальная конфигурация

`include/secrets.h` уже создан локально и содержит Wi-Fi, `CODEC_KEY`,
`device_id` и адрес адаптера. Файл исключён из Git:

```sh
git check-ignore -v firmware/esp32/include/secrets.h
```

Шаблон для другой машины — `include/secrets.example.h`. `ADAPTER_URL` должен
указывать на REST-путь устройства:

```c
#define CODEC_KEY "0123456789abcdef0123456789abcdef"
#define DEVICE_ID 1
#define ADAPTER_URL "http://192.168.0.197:8081/devices/1/telemetry"
```

`CODEC_KEY` — ровно 32 ASCII-байта из `custom_config.codec_key` в
`build/config/AdapterConfig.json`, без hex-декодирования. Это проверяется
`static_assert` при сборке. Ключ и Wi-Fi пароль не выводятся в Serial.

## Кодек

Алгоритм полностью повторяет `http_adapter/src/codec/Encoder.cpp`:

1. случайный IV 16 байт;
2. AES-256-CBC + PKCS#7;
3. `blob = IV || ciphertext`;
4. CRC32 IEEE 802.3 от `blob`, 8 lowercase hex;
5. Base64 RFC 4648 с `=`;
6. `{"checksum":"...","ciphertext":"..."}`.

Адаптер декодирует конверт, добавляет `device_id` из URI и отправляет обычный
JSON в Core. Ответ адаптера тоже зашифрован.

## Поведение

- первое измерение выполняется сразу после подключения к Wi-Fi;
- затем замер раз в минуту;
- неудачный POST повторяется в следующем цикле с тем же конвертом;
- после двух неудач выполняется принудительный reconnect Wi-Fi;
- в Serial выводятся температура, открытый payload, размер конверта, HTTP-статус
  и сетевые события.

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
| `200` | Adapter принял конверт и передал показание в Core |
| `400` | ошибка валидации или декодирования конверта |
| `502` | ошибка downstream; **запись могла уже сохраниться** в PostgreSQL |
| `< 0` | сетевая ошибка или таймаут |

При `502` прошивка не повторяет POST. Проверьте историю через
`GET /devices/1/telemetry`, чтобы не создать дубликат.

## Ограничения

- аутентификации устройства нет;
- HTTP без TLS (шифруется только тело запроса/ответа кодеком);
- нет идемпотентности: повторная отправка создаёт новую строку;
- IP Adapter `192.168.0.197` может измениться по DHCP.

## Диагностика

- `400 Invalid JSON in Adapter` — отсутствует `Content-Type: application/json`
  (в прошивке он задан);
- `400 Decode failed: ...` — не совпал `CODEC_KEY` или повреждён конверт;
- `400 Decoded payload must contain finite numeric temperature` — проверьте payload;
- `502` — не запущены Core/Rule Engine/Notification Service;
- `[wifi] connection timeout` — сеть 2.4 ГГц, неверный пароль или слабый сигнал;
- `[sensor] no DS18B20 found` — проверить GPIO 4, питание и резистор 4.7 кОм.
