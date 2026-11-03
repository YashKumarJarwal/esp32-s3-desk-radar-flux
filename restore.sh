#!/usr/bin/env bash
# SINGLE-POINT REVERT: writes the stock full-flash backup back at offset 0.
# Usage: ./restore.sh [backup.bin] [port]
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)/backups"
IMG="${1:-$DIR/stock-full-LATEST.bin}"
PORT="${2:-/dev/ttyACM0}"
ESPT="python3 -m esptool"; export PYTHONPATH="$(cd "$(dirname "$0")" && pwd)/tools"
[ -f "$IMG" ] || { echo "Backup not found: $IMG"; exit 1; }
[ -f "$IMG.sha256" ] && (cd "$(dirname "$IMG")" && sha256sum -c "$(basename "$IMG").sha256")
echo "== Erasing and restoring $IMG to $PORT"
$ESPT --port "$PORT" --chip esp32s3 write_flash --flash_mode keep --flash_size keep 0x0 "$IMG"
echo "== Restore complete. Device should reboot into stock firmware."
echo "   If the port vanished: hold BOOT, tap RESET (or re-plug USB while holding BOOT), then re-run."
