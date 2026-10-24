#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_setup
port="${1:-}"
if [ ! -c "$port" ]; then
  if [ -z "$port" ]; then
    echo 'Usage: bash scripts/run.sh /dev/cu.usbmodemXXXX' >&2
  else
    echo "Serial port not found: $port" >&2
  fi
  python3 - <<'PY' >&2
import glob
import os
import shlex
import stat
patterns = ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/ttyACM*", "/dev/ttyUSB*")
ports = sorted({p for pattern in patterns for p in glob.glob(pattern)
                if stat.S_ISCHR(os.stat(p).st_mode)})
if ports:
    print("Available USB serial ports (use the one connected to your Waveshare):")
    for candidate in ports:
        print("  bash scripts/run.sh " + shlex.quote(candidate))
else:
    print("No USB serial ports found. Connect the board with a USB data cable and turn it on.")
PY
  exit 1
fi
bash "$ECHO_ROOT/scripts/build.sh"

esptool="$(python3 - "$WAVESHARE_ROOT" <<'PY'
import os
import pathlib
import sys
root = pathlib.Path(sys.argv[1]) / ".arduino/data/packages/esp32/tools/esptool_py"
executables = sorted(path for path in root.glob("*/esptool") if os.access(path, os.X_OK))
if not executables:
    raise SystemExit("The installed ESP32 esptool executable was not found.")
print(executables[-1])
PY
)"
identity="$ECHO_ROOT/.build/device-identity.txt"
"$esptool" --chip esp32s3 --port "$port" chip_id | tee "$identity"
device_mac="$(python3 - "$identity" <<'PY'
import pathlib
import re
import sys
match = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", pathlib.Path(sys.argv[1]).read_text())
if not match:
    raise SystemExit("The ESP32-S3 did not report its MAC address; upload cancelled.")
print(match.group(1).replace(":", "").lower())
PY
)"
mkdir -p "$WAVESHARE_ROOT/backups"
backup="$WAVESHARE_ROOT/backups/$device_mac-original.bin"
if [ ! -f "$backup" ]; then
  echo 'Saving the connected board’s 16 MB flash before uploading...'
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
python3 - "$backup" <<'PY'
import pathlib
import sys
if pathlib.Path(sys.argv[1]).stat().st_size != 16777216:
    raise SystemExit("The existing flash backup is incomplete; upload cancelled.")
PY
shasum -a 256 -c "$backup.sha256"
waveshare_cli upload --fqbn "$WAVESHARE_FQBN" --port "$port" \
  --input-dir "$ECHO_ROOT/.build/out" "$ECHO_ROOT/.build/sketch/echo_game"
echo 'Uploaded Echo. Tap PLAY, then tap a direction to send a call.'
