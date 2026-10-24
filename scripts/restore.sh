#!/bin/bash
# Restore only the original backup belonging to the connected ESP32-S3.
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_setup
port="${1:-}"
if [ ! -c "$port" ]; then
  echo 'Usage: bash scripts/restore.sh /dev/cu.usbmodemXXXX' >&2
  exit 1
fi

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
identity="$("$esptool" --chip esp32s3 --port "$port" chip_id)"
device_mac="$(printf '%s\n' "$identity" | python3 -c '
import re, sys
match = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", sys.stdin.read())
if not match:
    raise SystemExit("The connected ESP32-S3 did not report its MAC address.")
print(match.group(1).replace(":", "").lower())
')"
backup="$WAVESHARE_ROOT/backups/$device_mac-original.bin"
python3 - "$backup" <<'PY'
import hashlib
import pathlib
import sys
backup = pathlib.Path(sys.argv[1])
checksum = pathlib.Path(str(backup) + ".sha256")
if not backup.is_file() or not checksum.is_file():
    raise SystemExit("No original backup exists for this device; restore cancelled.")
if backup.stat().st_size != 16777216:
    raise SystemExit("The original flash backup has the wrong size; restore cancelled.")
expected = checksum.read_text().split()[0]
if hashlib.sha256(backup.read_bytes()).hexdigest() != expected:
    raise SystemExit("The original flash backup checksum failed; restore cancelled.")
PY
echo 'Restoring the original firmware and flash contents...'
"$esptool" --chip esp32s3 --port "$port" --baud 460800 write_flash 0 "$backup"
echo 'Original flash restored. Power cycle the device if it remains in download mode.'
