#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
mkdir -p "$ECHO_ROOT/.build"
native_flags=(-std=c++11 -Wall -Wextra -Werror -O2)
if [ "$(uname -s)" = Darwin ]; then
  native_flags+=(-isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1")
fi
c++ "${native_flags[@]}" -I "$ECHO_ROOT/firmware/echo_game" \
  "$ECHO_ROOT/tests/benchmark.cpp" "$ECHO_ROOT/firmware/echo_game/Benchmark.cpp" \
  "$ECHO_ROOT/firmware/echo_game/EchoEngine.cpp" "$ECHO_ROOT/firmware/echo_game/EchoLevels.cpp" \
  "$ECHO_ROOT/firmware/echo_game/EchoApp.cpp" "$ECHO_ROOT/firmware/echo_game/Raster.cpp" \
  -o "$ECHO_ROOT/.build/benchmark-native"
"$ECHO_ROOT/.build/benchmark-native" > "$ECHO_ROOT/.build/benchmark-native.txt"
python3 "$ECHO_ROOT/scripts/check_speed.py" --report "$ECHO_ROOT/.build/benchmark-native.txt"
