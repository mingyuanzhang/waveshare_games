#!/bin/bash
# Build, back up the connected ESP32-S3, and upload the selected sketch.
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_setup

target="${1:-}"
port="${2:-}"
case "$target" in
  demo|timer|timer-test) ;;
  *) echo 'Usage: bash scripts/run.sh demo|timer|timer-test /dev/cu.usbmodemXXXX' >&2; exit 1 ;;
esac
if [ ! -c "$port" ]; then
  echo 'Supply the connected board serial port as the second argument.' >&2
  exit 1
fi

bash "$WAVESHARE_ROOT/scripts/build.sh" "$target"

esptool="$(python3 - "$WAVESHARE_ROOT" <<'PY'
import os
import pathlib
import sys
root = pathlib.Path(sys.argv[1]) / ".arduino/data/packages/esp32/tools/esptool_py"
tools = sorted(path for path in root.glob("*/esptool") if os.access(path, os.X_OK))
if not tools:
    raise SystemExit("The ESP32 package's esptool executable was not found.")
print(tools[-1])
PY
)"

identity="$WAVESHARE_ROOT/.build/device-identity.txt"
"$esptool" --chip esp32s3 --port "$port" chip_id | tee "$identity"
device_mac="$(python3 - "$identity" <<'PY'
import pathlib
import re
import sys
text = pathlib.Path(sys.argv[1]).read_text()
match = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", text)
if not match:
    raise SystemExit("The connected ESP32-S3 did not report its MAC address.")
print(match.group(1).replace(":", "").lower())
PY
)"

mkdir -p "$WAVESHARE_ROOT/backups"
backup="$WAVESHARE_ROOT/backups/$device_mac-original.bin"
if [ ! -f "$backup" ]; then
  echo 'Saving the original 16 MB flash before uploading...'
  "$esptool" --chip esp32s3 --port "$port" --baud 460800 \
    read_flash 0 16777216 "$backup.partial"
  python3 - "$backup.partial" <<'PY'
import pathlib
import sys
if pathlib.Path(sys.argv[1]).stat().st_size != 16777216:
    raise SystemExit("The flash backup is incomplete; upload cancelled.")
PY
  mv "$backup.partial" "$backup"
  shasum -a 256 "$backup" > "$backup.sha256"
fi
shasum -a 256 -c "$backup.sha256"

sketch_name=focus_timer
if [ "$target" = demo ]; then sketch_name=01_lvgl_demo; fi
waveshare_cli upload --fqbn "$WAVESHARE_FQBN" --port "$port" \
  --input-dir "$WAVESHARE_ROOT/.build/out/$target" \
  "$WAVESHARE_ROOT/.build/sketches/$target/$sketch_name"
echo "Uploaded $target. Check the device screen."

