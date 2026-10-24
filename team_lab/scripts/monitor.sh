#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
port="$(round_serial_port "${1:-}")"
waveshare_cli monitor --port "$port" --config baudrate=115200
