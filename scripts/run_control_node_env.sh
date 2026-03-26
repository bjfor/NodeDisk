#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${1:-$ROOT_DIR/configs/control_node.env}"

if [[ ! -f "$ENV_FILE" ]]; then
    echo "env file not found: $ENV_FILE" >&2
    exit 1
fi

set -a
source "$ENV_FILE"
set +a

exec "$ROOT_DIR/scripts/run_control_node.sh" \
    "${NODEDISK_CONTROL_DB_PATH:-$ROOT_DIR/output/control_node.db}" \
    "${NODEDISK_CONTROL_HOST:-0.0.0.0}" \
    "${NODEDISK_CONTROL_PORT:-18770}" \
    "${NODEDISK_OFFLINE_AFTER_SECONDS:-300}"
