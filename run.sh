#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

# 1. Локальный .env: создаём из шаблона и генерируем секреты, если их нет.
if [ ! -f .env ]; then
  cp .env.example .env
  if grep -q 'replace-with' .env; then
    pepper="$(openssl rand -hex 32)"
    jwt_secret="$(openssl rand -hex 32)"
    sed -i "s|replace-with-random-hex-at-least-32-characters|${jwt_secret}|" .env
    sed -i "s|replace-with-random-hex|${pepper}|" .env
  fi
  echo "Создан .env со сгенерированными секретами"
fi
if [ ! -f frontend/.env ]; then
  cp frontend/.env.example frontend/.env
  echo "Создан frontend/.env"
fi

set -a
# shellcheck disable=SC1091
source .env
set +a

# 2. Сборка.
cmake -S . -B build
cmake --build build --parallel

# 3. PostgreSQL и схема.
docker compose up -d
until docker compose exec -T postgres pg_isready -U postgres -d iot_platform >/dev/null 2>&1; do
  sleep 1
done
for attempt in {1..60}; do
  if (echo > /dev/tcp/127.0.0.1/5433) >/dev/null 2>&1; then break; fi
  if [ "$attempt" -eq 60 ]; then
    echo "127.0.0.1:5433 недоступен" >&2
    exit 1
  fi
  sleep 1
done
if ! docker compose exec -T postgres psql -U postgres -d postgres -tAc \
  "SELECT 1 FROM pg_database WHERE datname='iot_platform'" | grep -q 1; then
  docker compose exec -T postgres psql -U postgres -d postgres -c "CREATE DATABASE iot_platform;"
fi
docker compose run --rm db-init

# 4. Освобождаем порты сервисов от прошлых запусков.
for port in 8080 8081 8082 8083; do
  fuser -k "${port}/tcp" >/dev/null 2>&1 || true
done
sleep 1

PIDS=()
start_service() {
  local name="$1"; shift
  "$@" &
  local pid=$!
  PIDS+=("$pid")
  sleep 1
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "${name} завершился сразу после запуска" >&2
    return 1
  fi
}

cleanup() {
  echo
  echo "Остановка сервисов..."
  for pid in "${PIDS[@]:-}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  docker compose down
}
trap cleanup INT TERM EXIT

start_service "iot_core" ./build/bin/iot_core
start_service "http_adapter" ./build/bin/http_adapter
start_service "rule_engine" ./build/bin/rule_engine
start_service "notification_service" ./build/bin/notification_service

for port in 8080 8081 8082 8083; do
  for attempt in {1..30}; do
    if (echo > "/dev/tcp/127.0.0.1/${port}") >/dev/null 2>&1; then break; fi
    if [ "$attempt" -eq 30 ]; then
      echo "Порт ${port} не поднялся" >&2
      exit 1
    fi
    sleep 1
  done
done

echo "Все сервисы запущены. Нажмите Ctrl+C для остановки."
wait
