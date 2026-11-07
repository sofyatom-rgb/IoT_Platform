#!/usr/bin/env bash
set -Eeuo pipefail

# Run real repository integration tests against a dedicated disposable database.
# This script never drops or writes to the application's iot_platform database.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

readonly TEST_DB="iot_platform_test"
readonly MAIN_DB="iot_platform"
readonly TEST_CONNECTION="host=127.0.0.1 port=5433 dbname=${TEST_DB} user=postgres password=postgres"

if [[ "$TEST_DB" == "$MAIN_DB" ]]; then
  echo "Refusing to run: test database name must differ from the application database." >&2
  exit 2
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "Docker is required. Install Docker Engine and the Compose plugin first." >&2
  exit 2
fi
if ! docker compose version >/dev/null 2>&1; then
  echo "Docker Compose v2 is required (the 'docker compose' command)." >&2
  exit 2
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake is required." >&2
  exit 2
fi

RESET=0
case "${1:-}" in
  "") ;;
  --reset) RESET=1 ;;
  *)
    echo "Usage: $0 [--reset]" >&2
    echo "  --reset recreates ONLY the disposable ${TEST_DB} database." >&2
    exit 2
    ;;
esac

echo "[1/5] Starting PostgreSQL container..."
docker compose up -d postgres

echo "[2/5] Waiting for PostgreSQL health..."
ready=0
for _ in $(seq 1 60); do
  if docker compose exec -T postgres pg_isready -U postgres -d "$MAIN_DB" >/dev/null 2>&1; then
    ready=1
    break
  fi
  sleep 1
done
if [[ "$ready" -ne 1 ]]; then
  echo "PostgreSQL did not become ready within 60 seconds." >&2
  docker compose logs --tail=80 postgres >&2 || true
  exit 1
fi

if [[ "$RESET" -eq 1 ]]; then
  echo "[3/5] Recreating disposable database ${TEST_DB}..."
  docker compose exec -T postgres dropdb -U postgres --if-exists "$TEST_DB"
  docker compose exec -T postgres createdb -U postgres "$TEST_DB"
else
  echo "[3/5] Ensuring disposable database ${TEST_DB} exists..."
  exists="$(docker compose exec -T postgres psql -U postgres -d postgres -tAc \
    "SELECT 1 FROM pg_database WHERE datname = '${TEST_DB}'" | tr -d '[:space:]')"
  if [[ "$exists" != "1" ]]; then
    docker compose exec -T postgres createdb -U postgres "$TEST_DB"
  fi
fi

echo "Applying init.sql to ${TEST_DB}..."
docker compose exec -T postgres psql -U postgres -d "$TEST_DB" \
  -v ON_ERROR_STOP=1 < init.sql

echo "[4/5] Configuring and building repository integration tests..."
cmake -S . -B build-tests \
  -DBUILD_APPLICATION=OFF \
  -DBUILD_TESTING=ON \
  -DBUILD_CONTROLLER_TESTING=OFF \
  -DBUILD_POSTGRES_INTEGRATION_TESTING=ON \
  -DBUILD_CODEC_TESTING=OFF

cmake --build build-tests --target repository_integration_tests -j"$(nproc)"

echo "[5/5] Running repository integration tests against ${TEST_DB}..."
IOT_TEST_DATABASE_URL="$TEST_CONNECTION" \
  ctest --test-dir build-tests \
    -R '^repository_integration_tests$' \
    --output-on-failure -V

echo "PostgreSQL repository integration tests finished."
