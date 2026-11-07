#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 recipient@example.com" >&2
  exit 2
fi

TO="$1"
NOTIFY_URL="${NOTIFY_URL:-http://127.0.0.1:8082}"

echo "POST $NOTIFY_URL/notifications/email/test -> $TO"
curl --fail-with-body -sS -X POST "$NOTIFY_URL/notifications/email/test" \
  --data-urlencode "email=$TO"
echo
