#!/usr/bin/env bash
set -euo pipefail

NAME="mailpit"
IMAGE="axllent/mailpit"

case "${1:-up}" in
  up)
    if docker ps -a --format '{{.Names}}' | grep -qx "$NAME"; then
      docker start "$NAME" >/dev/null
    else
      docker run -d --name "$NAME" -p 1025:1025 -p 8025:8025 "$IMAGE" >/dev/null
    fi
    echo "Mailpit: SMTP 127.0.0.1:1025, UI http://127.0.0.1:8025"
    ;;
  down)
    docker rm -f "$NAME" >/dev/null 2>&1 || true
    echo "Mailpit stopped"
    ;;
  logs)
    docker logs -f "$NAME"
    ;;
  *)
    echo "Usage: $0 {up|down|logs}" >&2
    exit 1
    ;;
esac
