#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_setup
sketch="$(python3 "$ECHO_ROOT/scripts/stage.py")"
mkdir -p "$ECHO_ROOT/.build/out" "$ECHO_ROOT/.build/cache"
waveshare_cli compile --fqbn "$WAVESHARE_FQBN" --jobs 4 \
  --build-property 'compiler.optimization_flags=-O2' \
  --build-path "$ECHO_ROOT/.build/cache" \
  --output-dir "$ECHO_ROOT/.build/out" "$sketch"
