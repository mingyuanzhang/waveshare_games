#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_setup

target="${1:-timer}"
sketch="$(python3 "$WAVESHARE_ROOT/scripts/stage.py" "$target")"
include_flags="-DLV_CONF_INCLUDE_SIMPLE -I\"$sketch\""
cpp_flags="$include_flags"
if [ "$target" = timer-test ]; then
  cpp_flags="$cpp_flags -DFOCUS_TIMER_QUICK_TEST=1"
fi

mkdir -p "$WAVESHARE_ROOT/.build/out/$target" "$WAVESHARE_ROOT/.build/cache/$target"
waveshare_cli compile --fqbn "$WAVESHARE_FQBN" --jobs 4 \
  --build-path "$WAVESHARE_ROOT/.build/cache/$target" \
  --build-property "compiler.c.extra_flags=$include_flags" \
  --build-property "compiler.cpp.extra_flags=$cpp_flags" \
  --output-dir "$WAVESHARE_ROOT/.build/out/$target" "$sketch"

