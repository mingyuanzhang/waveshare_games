#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_setup
port="${1:-}"
if [ ! -c "$port" ]; then
  echo 'Usage: bash scripts/monitor.sh /dev/cu.usbmodemXXXX' >&2
  exit 1
fi
waveshare_cli monitor --port "$port" --config baudrate=115200

