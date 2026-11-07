#!/usr/bin/env bash
set -euo pipefail

TO="${1:-owner@example.com}"
NOTIFY_URL="${NOTIFY_URL:-http://127.0.0.1:8082}"

echo "POST $NOTIFY_URL/notifications/temperature -> $TO"
curl -sS -X POST "$NOTIFY_URL/notifications/temperature" \
  -H 'Content-Type: application/json' \
  -d "{\"email\":\"$TO\",\"device_name\":\"Smoke <sensor>\",\"temperature\":31.5,\"threshold\":30.0}"
echo
