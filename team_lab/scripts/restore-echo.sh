#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
mkdir -p "$ROUND_ROOT/.build"
port="$(round_serial_port "${1:-}")"
echo "Using USB serial port: $port"
esptool="$(python3 - "$WAVESHARE_ROOT" <<'PY'
import pathlib,sys,os
tools=sorted(p for p in (pathlib.Path(sys.argv[1])/'.arduino/data/packages/esp32/tools/esptool_py').glob('*/esptool') if os.access(p,os.X_OK))
if not tools:raise SystemExit('Installed esptool not found')
print(tools[-1])
PY
)"
identity="$ROUND_ROOT/.build/restore-identity.txt"
"$esptool" --chip esp32s3 --port "$port" chip_id | tee "$identity"
device_mac="$(python3 - "$identity" <<'PY'
import pathlib,re,sys
match=re.search(r'MAC:\s*([0-9a-fA-F:]{17})',pathlib.Path(sys.argv[1]).read_text())
if not match:raise SystemExit('Cannot identify board; restore cancelled')
print(match.group(1).replace(':','').lower())
PY
)"
backup="$WAVESHARE_ROOT/backups/$device_mac-before-round-wonders.bin"
if [ -f "$backup" ]; then
  python3 - "$backup" <<'PY'
import pathlib,sys
if pathlib.Path(sys.argv[1]).stat().st_size!=6356992:raise SystemExit('Incomplete Echo snapshot; restore cancelled')
PY
  (cd "$WAVESHARE_ROOT/backups" && shasum -a 256 -c "$device_mac-before-round-wonders.bin.sha256")
  # Restore the old data region before overlaying the preserved Echo image.
  full_backup="$WAVESHARE_ROOT/backups/$device_mac-before-round-wonders-v030-full.bin"
  if [ -f "$full_backup" ]; then
    python3 - "$full_backup" <<'CHECK'
import pathlib,sys
if pathlib.Path(sys.argv[1]).stat().st_size!=16777216:raise SystemExit('Incomplete full-flash snapshot; restore cancelled')
CHECK
    (cd "$WAVESHARE_ROOT/backups" && shasum -a 256 -c "$device_mac-before-round-wonders-v030-full.bin.sha256")
    "$esptool" --chip esp32s3 --port "$port" --baud 460800 write_flash 0 "$full_backup"
  fi
  "$esptool" --chip esp32s3 --port "$port" --baud 460800 write_flash 0 "$backup"
else
  echo 'No device-specific Echo snapshot; rebuilding Echo from its preserved source.'
  bash "$WAVESHARE_ROOT/echo_game/scripts/run.sh" "$port"
fi
