# FLUX — custom firmware for the ESP32-S3 desk flight tracker

Round 1.28" GC9A01 desk orb (ESP32-S3, 4 MB flash, 2 MB PSRAM). Stock Chinese firmware replaced by FLUX,
keeping every stock feature and adding modes, a modern bilingual dashboard, OTA, and a PC integration bridge.

## Layout
- `backups/`  — verified 4 MB dump of the stock firmware (`stock-full-LATEST.bin` + sha256)
- `restore.sh` — SINGLE-POINT REVERT: writes the stock image back (`./restore.sh`, ~20 s)
- `backup.sh`  — full-flash backup with double-read verification; `status.sh` live progress bar
- `tools/`     — esptool 4.7 with the ESP32-S3 stub (apt's copy lacks it)
- `firmware/`  — PlatformIO project (`venv/bin/pio run -t upload` from inside it)
- `firmware/web/index.html` — dashboard source; run `dev/embed_web.py` after editing
- `dev/`       — `mon.py` serial capture, `mirror.py` screen grab to PNG, `embed_web.py`
- `bridge/`    — PC bridge: `claude_hook.sh` (Claude Code hooks), `flux_bridge.py` + `plugins/` (spotify, github, system, command), `install.sh` (systemd user unit)

## Device
- Dashboard: http://192.168.1.26/ (or http://flux.local/) — Overview, Radar, Display, Data, WiFi, Integrations, Advanced, System
- Button: short = next theme · double = next range · triple = next mode · long = settings QR · hold at power-on = setup hotspot
- Modes: SKY (ADS-B radar with runways, arrival/departure chevrons, lock-on with route + closest approach) · FLIGHT (one aircraft: route arc on a zoomed coastline globe, big figures, altitude trace; short = next aircraft, double = page) · CHRONO (orbital clock) · ATMOS (weather + AQI) · ORBIT (ISS on a coastline globe) · SIGNAL (BLE/WiFi radar) · PULSE (integration cards)
- Geo data: 2,911 airports / 4,165 runways (OurAirports) and Natural Earth 110m coastlines compiled into flash by `dev/gen_geodata.py` (inputs in `dev/geodata/`, not committed)
- Serial 115200: `h` for debug commands (`u` theme, `r` range, `n` next mode, `a` API status, `p` settings, `b` reboot, `1-4` virtual presses)

## Push API (PULSE)
```
curl -X POST http://192.168.1.26/api/pulse -H 'Content-Type: application/json' \
  -d '{"id":"demo","icon":"ai","title":"Hello","text":"from curl","style":"pulse","color":"#2dd4bf","overlay":true,"takeover":8,"ttl":60}'
```
Fields: id, icon (ai|github|music|bell|check|warn|ci|dot), title, text, style (static|pulse|think|play), color (#rrggbb),
progress (0-100 or -1), overlay (badge on any screen), takeover (seconds of full screen), priority, ttl (seconds).
Album art: POST `{"id":"<id>","b64":"<base64 of 64x64 RGB565, 8192 bytes big-endian>"}` to `/api/pulse/image`. Optional `X-Token` header if a push token is set.

## OTA
System tab → upload `firmware/.pio/build/flux/firmware.bin`. Written to the spare slot; auto-rollback if the new image never marks itself valid.
USB flashing always works regardless: `cd firmware && ../venv/bin/pio run -t upload`.

## Revert to stock
`./restore.sh` — that's it. The ESP32 ROM bootloader cannot be overwritten by any of this, so the device cannot be bricked.
