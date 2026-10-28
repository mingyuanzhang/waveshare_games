#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_setup
node "$ROUND_ROOT/../../wonder-audio/scripts/export-embedded.cjs" "$ROUND_ROOT/.build/assets"
sketch="$(python3 "$ROUND_ROOT/scripts/stage.py")"
mkdir -p "$ROUND_ROOT/.build/cache" "$ROUND_ROOT/.build/out"
waveshare_cli compile --fqbn "$WAVESHARE_FQBN" --jobs 4 \
  --build-property 'compiler.optimization_flags=-O2' --build-property 'upload.maximum_size=10485760' \
  --build-path "$ROUND_ROOT/.build/cache" --output-dir "$ROUND_ROOT/.build/out" "$sketch"
