#!/usr/bin/env bash
# Full flash backup of the desk flight tracker (ESP32-C3, 4 MB).
# Reads the ENTIRE flash twice, verifies both reads match, stores with sha256.
set -euo pipefail
PORT="${1:-/dev/ttyACM0}"
DIR="$(cd "$(dirname "$0")" && pwd)/backups"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT="$DIR/stock-full-$STAMP.bin"
ESPT="python3 -m esptool"; export PYTHONPATH="$(cd "$(dirname "$0")" && pwd)/tools"
[ -n "$ESPT" ] || { echo "esptool not found; sudo apt-get install -y esptool"; exit 1; }

echo "== Chip info"
$ESPT --port "$PORT" --chip esp32s3 flash_id | tee "$DIR/flash_id-$STAMP.txt"
SIZE_HEX=$(grep -oE 'Detected flash size: [0-9]+MB' "$DIR/flash_id-$STAMP.txt" | grep -oE '[0-9]+' || echo 4)
SIZE=$((SIZE_HEX * 1024 * 1024))
echo "== Reading full flash ($SIZE bytes) -> $OUT"
$ESPT --port "$PORT" --chip esp32s3 read_flash 0 "$SIZE" "$OUT"
echo "== Second read for verification"
$ESPT --port "$PORT" --chip esp32s3 read_flash 0 "$SIZE" "$OUT.verify"
if cmp -s "$OUT" "$OUT.verify"; then
  rm "$OUT.verify"
  (cd "$DIR" && sha256sum "$(basename "$OUT")" > "$(basename "$OUT").sha256")
  cp "$OUT" "$DIR/stock-full-LATEST.bin"; cp "$OUT.sha256" "$DIR/stock-full-LATEST.bin.sha256"
  echo "== BACKUP OK: $OUT"; cat "$OUT.sha256"
else
  echo "!! Two reads differ. Do NOT trust this backup. Re-run."; exit 2
fi
