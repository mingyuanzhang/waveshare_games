#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
# Upload the regular all-level game, then time its real call and display paths.
bash "$ECHO_ROOT/scripts/run.sh" "${1:-}"
python3 "$ECHO_ROOT/scripts/check_speed.py" "$1"
