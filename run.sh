#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

# 1. Сборка проекта.
cmake -S . -B build
cmake --build build --parallel

# 2. Поднятие Docker Compose.
docker compose up -d

# Ждём готовности PostgreSQL.
until docker compose exec -T postgres pg_isready -U postgres -d iot_platform >/dev/null 2>&1; do
  sleep 1
done

# Core connects to the host-mapped PostgreSQL port, so also wait for that
# endpoint before starting services that immediately query the database.
for attempt in {1..60}; do
  if (echo > /dev/tcp/127.0.0.1/5433) >/dev/null 2>&1; then
    break
  fi
  if [ "$attempt" -eq 60 ]; then
    echo "PostgreSQL is ready in Compose but 127.0.0.1:5433 is not reachable" >&2
    exit 1
  fi
  sleep 1
done

# Гарантируем, что БД и схема созданы даже если база была удалена вручную.
if ! docker compose exec -T postgres psql -U postgres -d postgres -tAc \
  "SELECT 1 FROM pg_database WHERE datname='iot_platform'" | grep -q 1; then
  docker compose exec -T postgres psql -U postgres -d postgres -c \
    "CREATE DATABASE iot_platform;"
fi
docker compose run --rm db-init

# Переменные окружения нужны всем сервисам, поэтому грузим их до запуска.
if [ -f .env ]; then
  set -a
  # shellcheck disable=SC1091
  source .env
  set +a
fi

PIDS=()

free_port() {
  local port="$1"
  if fuser "${port}/tcp" >/dev/null 2>&1; then
    fuser -k "${port}/tcp" >/dev/null 2>&1 || true
    sleep 1
  fi
}

# Освобождаем порты, если предыдущие процессы ещё занимают их.
for port in 8080 8081 8082 8083; do
  free_port "$port"
done

cleanup() {
  echo
  echo "Остановка запущенных сервисов..."
  for pid in "${PIDS[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
    fi
  done
  wait 2>/dev/null || true
  docker compose down
}
trap cleanup INT TERM EXIT

# 3. IoT Core.
./build/bin/iot_core &
PIDS+=($!)
sleep 1

# 4. HTTP Adapter.
./build/bin/http_adapter &
PIDS+=($!)
sleep 1

# 5. Rule Engine.
./build/bin/rule_engine &
PIDS+=($!)
sleep 1

# 6. Notification Service.
./build/bin/notification_service &
PIDS+=($!)

# Ждём, пока все сервисы реально начнут слушать порты.
for port in 8080 8081 8082 8083; do
  until (echo > "/dev/tcp/127.0.0.1/${port}") >/dev/null 2>&1; do
    sleep 1
  done
done

echo "Все сервисы запущены. Нажмите Ctrl+C для остановки."
wait