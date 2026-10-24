#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
node "$ROUND_ROOT/../../wonder-audio/scripts/export-embedded.cjs" "$ROUND_ROOT/.build/assets"
mkdir -p "$ROUND_ROOT/.build/preview"
flags=(-std=c++17 -Wall -Wextra -Werror -O2)
if [ "$(uname -s)" = Darwin ]; then flags+=(-isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1"); fi
c++ "${flags[@]}" -I "$ROUND_ROOT/firmware/round_wonders" -I "$ROUND_ROOT/.build/assets" -I "$ROUND_ROOT/../echo_game/firmware/echo_game" \
 "$ROUND_ROOT/tests/probe.cpp" "$ROUND_ROOT/firmware/round_wonders/RoundWorld.cpp" \
 "$ROUND_ROOT/firmware/round_wonders/RoundRenderer.cpp" "$ROUND_ROOT/firmware/round_wonders/Music.cpp" \
 "$ROUND_ROOT/../echo_game/firmware/echo_game/Raster.cpp" -o "$ROUND_ROOT/.build/probe"
"$ROUND_ROOT/.build/probe" "$ROUND_ROOT/.build/assets/audio.bin" "$ROUND_ROOT/.build/preview"
python3 "$ROUND_ROOT/scripts/preview.py"
