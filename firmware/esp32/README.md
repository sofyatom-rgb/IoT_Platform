# ESP32 DS18B20 → IoT Platform HTTP Adapter

Минимальная прошивка: ESP32 DevKit читает DS18B20, формирует JSON, шифрует его
тем же кодеком, что и HTTP Adapter, и отправляет `POST /ingest`.

Один цикл выполняется при старте платы. Автоматических повторных POST нет:
для повторного замера перезагрузите плату.

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
│   ├── main.cpp            # Wi-Fi, DS18B20, HTTP
│   ├── codec.cpp           # AES-256-CBC, PKCS#7, CRC32, Base64
│   └── codec.h
├── test/test_codec.cpp     # тесты кодека на компьютере
└── tools/run_native_tests.sh
```

## Локальная конфигурация

`include/secrets.h` уже создан локально и содержит Wi-Fi, ключ кодека,
`device_id` и адрес Adapter. Файл исключён из Git:

```sh
git check-ignore -v firmware/esp32/include/secrets.h
```

Шаблон для другой машины — `include/secrets.example.h`.

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

На ESP32 используется mbedTLS, на компьютере — OpenSSL. Логика CRC32, Base64,
PKCS#7 и конверта общая.

## Тесты кодека на компьютере

```sh
cd firmware/esp32
./tools/run_native_tests.sh
```

Проверяется:

- Base64 RFC 4648 и отказ неверного padding;
- CRC32 `"123456789"` → `cbf43926`;
- детерминированный вектор с тестовым ключом и фиксированным IV;
- round-trip со случайным IV для длин 1…64;
- границы PKCS#7 (15/16/17/31/32/33);
- повреждённый checksum, неверный ключ, битый Base64, отсутствующие поля.

Эталонный вектор (тестовый ключ, не серверный):

```text
plaintext  {"device_id":123,"temperature":23.75}
checksum   490570fe
```

Совместимость с реальным ключом проверяется локально без вывода ключа:

```sh
cd /home/kleshch/Projects/IoT_Platform
KEY=$(python3 -c "import json;print(json.load(open('build/config/AdapterConfig.json'))['custom_config']['codec_key'])")
PT='{"device_id":1,"temperature":23.75}'
ENV=$(./build/bin/codec_cli encode "$KEY" "$PT")
# decode ENV кодеком прошивки и сравнить с PT
```

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

## Этап 1: без записи в БД

В `include/secrets.h`:

```c
#define TELEMETRY_WRITE_ENABLED 0
```

Плата подключается к Wi-Fi, читает датчик, строит и шифрует payload, печатает
его в Serial, но **не отправляет POST**. Wi-Fi после этого отключается.

Пример безопасного вывода:

```text
[boot] ESP32 DS18B20 telemetry client
[sensor] devices found: 1
[sensor] temperature: 23.75 C
[payload] {"device_id":1,"temperature":23.7500}
[codec] envelope (... bytes): {"checksum":"...","ciphertext":"..."}
[wifi] connected
[http] TELEMETRY_WRITE_ENABLED=0, POST skipped
[wifi] disconnected
```

Полный конверт печатать безопасно: он не содержит ключ. Его можно проверить
локально через `build/bin/codec_cli decode`.

## Этап 2: отправка

После отдельного подтверждения:

```c
#define TELEMETRY_WRITE_ENABLED 1
```

Пересоберите и прошейте. Oдин POST за загрузку, без автоматических повторов.
В Serial выводятся только Wi-Fi статус, температура и HTTP-код.

## Побочные эффекты `device_id = 1`

Перед первой отправкой учтено:

- устройство `1` принадлежит пользователю `1` (`user2@example.com`);
- у пользователя задан Telegram chat id;
- правило устройства: `temperature != 12`;
- любое значение, кроме ровно `12`, включая комнатную температуру, запускает
  ветку уведомлений Rule Engine → Notification Service (Telegram и email,
  если каналы настроены);
- в таблице уже 23 записи для `device_id = 1`.

Поэтому первый реальный POST выполняется только после явного подтверждения.
Если нужна «тихая» проверка записи, лучше создать отдельное тестовое
устройство без правил.

## Коды HTTP

| Код | Значение |
|---|---|
| `200` | Adapter принял и передал в Core |
| `400` | ошибка формата/конверта/валидации |
| `502` | ошибка downstream; **запись могла уже сохраниться** в PostgreSQL |
| `< 0` | сетевая ошибка или таймаут |

При `502` прошивка не повторяет POST. Сначала проверьте телеметрию через
`POST /telemetry/query` (read-only), чтобы не создать дубликат.

## Ограничения

- общий ключ на все устройства, аутентификации устройства нет;
- CRC32 — контроль целостности, не подпись;
- HTTP без TLS;
- нет идемпотентности: повторная отправка создаёт новую строку;
- IP Adapter `192.168.0.197` может измениться по DHCP.

## Диагностика

- `400 Invalid JSON in Adapter` — отсутствует `Content-Type: application/json`
  (в прошивке он задан) или неверный ключ;
- `400 Decode failed` — ключ не совпадает с Adapter или ключ был hex-decoded;
- `502` — не запущены Core/Rule Engine/Notification Service;
- `[wifi] connection timeout` — сеть 2.4 ГГц, неверный пароль или слабый сигнал;
- `[sensor] no DS18B20 found` — проверить GPIO 4, питание и резистор 4.7 кОм.
