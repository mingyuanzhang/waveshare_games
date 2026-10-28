#!/bin/bash
# Back up both Echo application slots, settings, and boot information before replacing them.
set -euo pipefail
source "$(dirname "$0")/common.sh"
port="$(round_serial_port "${1:-}")"
echo "Using USB serial port: $port"
bash "$ROUND_ROOT/scripts/test.sh"
bash "$ROUND_ROOT/scripts/build.sh"
esptool="$(python3 - "$WAVESHARE_ROOT" <<'PY'
import pathlib,sys,os
tools=sorted(p for p in (pathlib.Path(sys.argv[1])/'.arduino/data/packages/esp32/tools/esptool_py').glob('*/esptool') if os.access(p,os.X_OK))
if not tools:raise SystemExit('Installed esptool not found')
print(tools[-1])
PY
)"
identity="$ROUND_ROOT/.build/device-identity.txt"
"$esptool" --chip esp32s3 --port "$port" chip_id | tee "$identity"
device_mac="$(python3 - "$identity" <<'PY'
import pathlib,re,sys
match=re.search(r'MAC:\s*([0-9a-fA-F:]{17})',pathlib.Path(sys.argv[1]).read_text())
if not match:raise SystemExit('Cannot identify board; upload cancelled')
print(match.group(1).replace(':','').lower())
PY
)"
# The expanded application overlaps part of the former FAT area. Preserve all
# flash before any upload, while keeping an older Echo-prefix snapshot intact.
backup="$WAVESHARE_ROOT/backups/$device_mac-before-round-wonders.bin"
full_backup="$WAVESHARE_ROOT/backups/$device_mac-before-round-wonders-v030-full.bin"
mkdir -p "$WAVESHARE_ROOT/backups"
if [ ! -f "$full_backup" ]; then
  "$esptool" --chip esp32s3 --port "$port" --baud 460800 read_flash 0 16777216 "$full_backup.partial"
  python3 - "$full_backup.partial" <<'CHECK'
import pathlib,sys
if pathlib.Path(sys.argv[1]).stat().st_size!=16777216:raise SystemExit('Incomplete full-flash backup; upload cancelled')
CHECK
  mv "$full_backup.partial" "$full_backup"
  (cd "$WAVESHARE_ROOT/backups" && shasum -a 256 "$device_mac-before-round-wonders-v030-full.bin" > "$device_mac-before-round-wonders-v030-full.bin.sha256")
fi
python3 - "$full_backup" <<'CHECK'
import pathlib,sys
if pathlib.Path(sys.argv[1]).stat().st_size!=16777216:raise SystemExit('Full-flash backup has wrong size; upload cancelled')
CHECK
(cd "$WAVESHARE_ROOT/backups" && shasum -a 256 -c "$device_mac-before-round-wonders-v030-full.bin.sha256")
if [ ! -f "$backup" ]; then
  python3 - "$full_backup" "$backup" <<'CHECK'
import pathlib,sys
pathlib.Path(sys.argv[2]).write_bytes(pathlib.Path(sys.argv[1]).read_bytes()[:6356992])
CHECK
  (cd "$WAVESHARE_ROOT/backups" && shasum -a 256 "$device_mac-before-round-wonders.bin" > "$device_mac-before-round-wonders.bin.sha256")
fi
python3 - "$backup" <<'CHECK'
import pathlib,sys
if pathlib.Path(sys.argv[1]).stat().st_size!=6356992:raise SystemExit('Existing Echo backup has wrong size; upload cancelled')
CHECK
(cd "$WAVESHARE_ROOT/backups" && shasum -a 256 -c "$device_mac-before-round-wonders.bin.sha256")
waveshare_cli upload --fqbn "$WAVESHARE_FQBN" --port "$port" --input-dir "$ROUND_ROOT/.build/out" "$ROUND_ROOT/.build/sketch/round_wonders"
echo 'Round Wonders installed. Action/BOOT: click changes shape, double-click changes style, hold 1 second toggles sound. Tap to place.'
