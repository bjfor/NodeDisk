#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${1:-$ROOT_DIR/configs/device_agent.env}"

if [[ ! -f "$ENV_FILE" ]]; then
    echo "env file not found: $ENV_FILE" >&2
    exit 1
fi

set -a
source "$ENV_FILE"
set +a

exec "$ROOT_DIR/scripts/run_device_agent.sh" \
    "${NODEDISK_AGENT_DB_PATH:-$ROOT_DIR/output/device_agent.db}" \
    "${NODEDISK_CONTROL_HOST:-127.0.0.1}" \
    "${NODEDISK_CONTROL_PORT:-18770}" \
    "${NODEDISK_NODE_ID:-node-local}" \
    "${NODEDISK_DEVICE_NAME:-$(hostname)}" \
    "${NODEDISK_ZT_IP:-127.0.0.1}" \
    "${NODEDISK_BACKEND_NAME:-mirror}" \
    "${NODEDISK_BACKEND_ARG:-$ROOT_DIR/output/store}" \
    "${NODEDISK_POLL_INTERVAL_SECONDS:-5}" \
    "${NODEDISK_MAX_CYCLES:-0}"
