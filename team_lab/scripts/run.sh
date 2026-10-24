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
backup="$WAVESHARE_ROOT/backups/$device_mac-before-round-wonders.bin"
mkdir -p "$WAVESHARE_ROOT/backups"
if [ ! -f "$backup" ]; then
  "$esptool" --chip esp32s3 --port "$port" --baud 460800 read_flash 0 6356992 "$backup.partial"
  python3 - "$backup.partial" <<'PY'
import pathlib,sys
if pathlib.Path(sys.argv[1]).stat().st_size!=6356992:raise SystemExit('Incomplete Echo backup; upload cancelled')
PY
  mv "$backup.partial" "$backup"
  (cd "$WAVESHARE_ROOT/backups" && shasum -a 256 "$device_mac-before-round-wonders.bin" > "$device_mac-before-round-wonders.bin.sha256")
fi
python3 - "$backup" <<'PY'
import pathlib,sys
if pathlib.Path(sys.argv[1]).stat().st_size!=6356992:raise SystemExit('Existing Echo backup has wrong size; upload cancelled')
PY
(cd "$WAVESHARE_ROOT/backups" && shasum -a 256 -c "$device_mac-before-round-wonders.bin.sha256")
waveshare_cli upload --fqbn "$WAVESHARE_FQBN" --port "$port" --input-dir "$ROUND_ROOT/.build/out" "$ROUND_ROOT/.build/sketch/round_wonders"
echo 'Round Wonders installed. Action/BOOT: click changes shape, double-click changes style, hold 1 second toggles sound. Tap to place.'
