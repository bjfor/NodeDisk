#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/output/control_node.db}"
HOST="${2:-0.0.0.0}"
PORT="${3:-18770}"
OFFLINE_AFTER_SECONDS="${4:-300}"

mkdir -p "$(dirname "$DB_PATH")"

exec "$ROOT_DIR/build/control_node" serve "$DB_PATH" "$HOST" "$PORT" "$OFFLINE_AFTER_SECONDS"
