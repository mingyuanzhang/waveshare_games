#!/bin/bash
set -euo pipefail
ROUND_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROUND_ROOT/../scripts/common.sh"

# Print only the chosen path on stdout, so callers can capture it safely.
round_serial_port() {
  python3 - "${1:-}" <<'PY'
import glob
from pathlib import Path
import sys

requested = sys.argv[1]
if requested and Path(requested).is_char_device():
    print(requested)
    raise SystemExit(0)
ports = sorted({p for pattern in (
    '/dev/cu.usbmodem*', '/dev/cu.usbserial*', '/dev/ttyACM*', '/dev/ttyUSB*'
) for p in glob.glob(pattern) if Path(p).is_char_device()})
if requested:
    print('Serial port not found: ' + requested, file=sys.stderr)
elif len(ports) == 1:
    print(ports[0])
    raise SystemExit(0)
if not ports:
    print('No USB serial port detected. Connect the powered-on board with a USB data cable, then try again.', file=sys.stderr)
else:
    print('Available USB serial ports:', file=sys.stderr)
    for port in ports:
        print('  ' + port, file=sys.stderr)
    print('Run again with the board\'s port, or omit the port when only one board is connected.', file=sys.stderr)
raise SystemExit(1)
PY
}
