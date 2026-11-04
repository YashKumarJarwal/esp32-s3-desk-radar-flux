#!/usr/bin/env bash
# Live backup progress, refreshed every 0.01 s. Ctrl-C to quit.
LOG="$(cd "$(dirname "$0")" && pwd)/backups/backup-run.log"
TOTAL=4194304
while true; do
  cur=$(grep -oE "[0-9]+ \([0-9]+ %\)" "$LOG" 2>/dev/null | tail -1 | cut -d' ' -f1)
  passes=$(grep -c "Stub running" "$LOG" 2>/dev/null)
  case "$passes" in 2) stage="PASS 1/2  reading";; 3) stage="PASS 2/2  verifying";; *) stage="connecting";; esac
  cur=${cur:-0}; pct=$((cur*100/TOTAL)); filled=$((pct/2))
  bar=$(printf '%*s' "$filled" '' | tr ' ' '#'); pad=$(printf '%*s' $((50-filled)) '')
  if grep -q "BACKUP OK" "$LOG" 2>/dev/null; then
    printf "\r\033[K[%s] 100%%  DONE: backup verified -> backups/stock-full-LATEST.bin\n" "$(printf '%50s' '' | tr ' ' '#')"; exit 0
  elif grep -qE "fatal|differ|exit=[1-9]" "$LOG" 2>/dev/null; then
    printf "\r\033[K FAILED - see %s\n" "$LOG"; exit 1
  fi
  printf "\r\033[K%-20s [%s%s] %3d%%  %'d / %'d bytes" "$stage" "$bar" "$pad" "$pct" "$cur" "$TOTAL"
  sleep 0.01
done
