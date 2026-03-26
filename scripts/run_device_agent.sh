#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCAL_DB_PATH="${1:-$ROOT_DIR/output/device_agent.db}"
CONTROL_HOST="${2:-127.0.0.1}"
CONTROL_PORT="${3:-18770}"
NODE_ID="${4:-node-local}"
DEVICE_NAME="${5:-$(hostname)}"
ZT_IP="${6:-127.0.0.1}"
BACKEND_NAME="${7:-mirror}"
BACKEND_ARG="${8:-$ROOT_DIR/output/store}"
POLL_INTERVAL_SECONDS="${9:-5}"
MAX_CYCLES="${10:-0}"

mkdir -p "$(dirname "$LOCAL_DB_PATH")"
if [[ "$BACKEND_NAME" == "mirror" ]]; then
    mkdir -p "$BACKEND_ARG"
fi

exec "$ROOT_DIR/build/device_agent" serve \
    "$LOCAL_DB_PATH" \
    "$CONTROL_HOST" \
    "$CONTROL_PORT" \
    "$NODE_ID" \
    "$DEVICE_NAME" \
    "$ZT_IP" \
    "$BACKEND_NAME" \
    "$BACKEND_ARG" \
    "$POLL_INTERVAL_SECONDS" \
    "$MAX_CYCLES"
