#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

TEST_ROOT="/tmp/netdisk_v1_test"
DB_PATH="$TEST_ROOT/netdisk.db"
BACKUP_SRC="$TEST_ROOT/source_backup"
BACKUP_STORE="$TEST_ROOT/store_backup"
SHARE_LIBRARY="$TEST_ROOT/share_library"
SHARE_RECEIVE="$TEST_ROOT/share_receive"

rm -rf "$TEST_ROOT"
mkdir -p "$BACKUP_SRC/docs" "$BACKUP_STORE" "$SHARE_LIBRARY" "$SHARE_RECEIVE"

cat > "$BACKUP_SRC/docs/important.txt" <<'EOF'
phase1-backup-payload
EOF

python3 - <<'PY'
from pathlib import Path
root = Path("/tmp/netdisk_v1_test/source_backup/docs")
root.joinpath("big.bin").write_bytes(b"A" * (5 * 1024 * 1024))
root.joinpath("handoff.txt").write_text("phase1-share-payload\n", encoding="utf-8")
PY

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" -j >/dev/null

echo "[test] backup-run"
"$BUILD_DIR/netdisk_cli" backup-run "$DB_PATH" node-a "$BACKUP_SRC" mirror "$BACKUP_STORE"

echo "[test] share-run"
"$BUILD_DIR/netdisk_cli" share-run "$DB_PATH" node-a entry-1 "$BACKUP_SRC/docs/handoff.txt" handoff.txt "$SHARE_LIBRARY" node-b "$SHARE_RECEIVE"

echo "[test] inspect"
INSPECT_OUTPUT="$("$BUILD_DIR/netdisk_cli" inspect "$DB_PATH")"
printf '%s\n' "$INSPECT_OUTPUT"

echo "[test] verify outputs"
test -n "$(find "$BACKUP_STORE" -type f -name '*.bin' -print -quit)"
test -f "$SHARE_LIBRARY/entry-1.txt"
test -f "$SHARE_RECEIVE/handoff.txt"
printf '%s\n' "$INSPECT_OUTPUT" | rg "^backup_records=3$" >/dev/null

echo "[ok] v1 test flow completed"
