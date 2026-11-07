# PostgreSQL repository integration tests

These tests execute the real `UsersRepository`, `DevicesRepository`, and
`TelemetryRepository` against PostgreSQL. They are integration tests, not
mock-based unit tests.

## Recommended: one command

From the project root:

```bash
./scripts/run-postgres-tests.sh
```

The script:

1. starts the existing `postgres` service from `docker-compose.yml`;
2. waits until PostgreSQL is ready;
3. creates the dedicated `iot_platform_test` database if it does not exist;
4. applies `init.sql` to that test database;
5. configures CMake with `BUILD_POSTGRES_INTEGRATION_TESTING=ON`;
6. builds and runs only `repository_integration_tests`.

It uses this connection by default:

```text
host=127.0.0.1 port=5433 dbname=iot_platform_test user=postgres password=postgres
```

The application's database is `iot_platform`; the integration tests use
`iot_platform_test`. The script does not drop, truncate, or run tests against
`iot_platform`.

### Recreate the disposable test database

To drop and recreate **only** `iot_platform_test`, then reapply the schema:

```bash
./scripts/run-postgres-tests.sh --reset
```

This deletes all data in `iot_platform_test`. Do not use `--reset` if you have
put anything valuable in that disposable test database. It does not delete the
Docker volume or the application's `iot_platform` database.

## Manual commands

Start PostgreSQL:

```bash
docker compose up -d postgres
```

Create the test database if needed:

```bash
docker compose exec postgres createdb -U postgres iot_platform_test
```

If it already exists, `createdb` reports an error; in that case skip this step.

Apply the schema:

```bash
docker compose exec -T postgres psql -U postgres -d iot_platform_test \
  -v ON_ERROR_STOP=1 < init.sql
```

Configure and build the test target (requires Drogon, OpenSSL, and JsonCpp
development packages):

```bash
cmake -S . -B build-tests \
  -DBUILD_APPLICATION=OFF \
  -DBUILD_TESTING=ON \
  -DBUILD_CONTROLLER_TESTING=ON \
  -DBUILD_POSTGRES_INTEGRATION_TESTING=ON \
  -DBUILD_CODEC_TESTING=ON

cmake --build build-tests --target repository_integration_tests -j$(nproc)
```

Run only the PostgreSQL tests:

```bash
IOT_TEST_DATABASE_URL="host=127.0.0.1 port=5433 dbname=iot_platform_test user=postgres password=postgres" \
  ctest --test-dir build-tests \
    -R '^repository_integration_tests$' \
    --output-on-failure -V
```

Run all configured tests, including the PostgreSQL suite:

```bash
ctest --test-dir build-tests --output-on-failure -V
```

## What the repository tests verify

- Users: insert/read/update/delete, Telegram chat ID handling, duplicate email
  constraint, and NULL Telegram ID.
- Devices: insert/read/update/delete, notification lookup, and not-found error.
- Telemetry: writing readings, retrieving the latest ten, deleting readings,
  and forwarding foreign-key violations to the error callback.
- Database schema: deleting a user cascades to that user's devices and their
  telemetry rows.

The tests generate unique email addresses and delete their fixture rows after
successful test execution. A test process interrupted by a crash may leave
fixture rows behind; these do not collide with later runs because email values
are unique. Use `--reset` to recreate the test database if necessary.

Never point `IOT_TEST_DATABASE_URL` at your normal application database or a
database containing important data.
