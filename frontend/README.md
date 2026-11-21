# IoT Monitor frontend

Russian-language mission-control SPA для IoT Platform.

## Запуск

Нужны Node 22+ и pnpm. Сначала поднимите бэкенд из корня проекта:

```sh
cd ..
./run.sh
```

Затем в отдельном терминале:

```sh
cd frontend
pnpm install
pnpm dev
```

`run.sh` создаст `frontend/.env` из `.env.example`, если файла нет. По умолчанию
используется live-режим: Vite проксирует `/api/core`, `/api/rules` и
`/api/notifications` на порты `8080`, `8083` и `8082` соответственно.

## Авторизация

Регистрация (`POST /users`) и вход (`POST /auth/login`) работают с реальным
бэкендом. JWT хранится в `sessionStorage`, пользователь — в `localStorage`.
Protected-маршрут требует действующий токен; после истечения токена или ответа
`401` фронтенд возвращает на экран входа.

Список устройств определяется токеном — передавать `owner_id` вручную не нужно.
Устройства без телеметрии показывают `--`. В mock-режиме (`VITE_API_MODE=mock`)
доступны демо-пользователи `demo@iot.local` / `demo1234` и
`admin@iot.local` / `admin1234`.

## Quality

```sh
pnpm lint
pnpm typecheck
pnpm test
pnpm build
```
