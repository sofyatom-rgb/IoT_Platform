# IoT Monitor frontend

Russian-language mission-control SPA for the IoT Platform. It runs independently of the C++ services in mock mode, with seeded Russian industrial devices, telemetry, authentication, devices, rules, telemetry, channel checks and profile controls.

## Run

Requires Node 22+ and pnpm.

```sh
pnpm install
pnpm dev
```

The default mode is mock. Demo users: `demo@iot.local` / `demo1234` and `admin@iot.local` / `admin1234`.

## Live mode

Copy `.env.example`, set `VITE_API_MODE=live`, configure the three base paths, and optionally set `VITE_LIVE_OWNER_ID` to an existing Core user id. Vite proxies `/api/core` to 8080, `/api/rules` to 8083, and `/api/notifications` to 8082. A user created through the registration screen is used as the owner automatically for that browser session; `VITE_LIVE_OWNER_ID` is useful after a refresh. The API boundary is in `src/lib/api/`.

The live adapter calls the C++ endpoints as they exist today: `POST/PUT/GET/DELETE /users`, `POST/PUT/GET/DELETE /devices`, `POST/DELETE /telemetry`, `POST /telemetry/query`, rule CRUD, and `POST /notifications/email/test`. Current backend gaps include no authentication, no user/device list, and a profile update which requires a password. Therefore live login remains unavailable, dashboards can only show devices created or opened during the browser session, and profile editing stays disabled. The adapter never renders or stores the password returned by `GET /users?id=`.

Use a production reverse proxy for `/api/core`, `/api/rules`, and `/api/notifications` to their internal services. Do not expose the notification service directly to public clients.

## Quality

```sh
pnpm lint
pnpm typecheck
pnpm test
pnpm build
```

Timezone-less server timestamps are parsed as UTC and rendered in MSK. The device status derivation is unit-tested. The visual network fallback avoids WebGL dependency and stays resilient on constrained devices.
