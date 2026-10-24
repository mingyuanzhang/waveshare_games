#!/bin/bash
set -euo pipefail
ECHO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ECHO_ROOT/scripts/sync-game.sh"
python3 "$ECHO_ROOT/tests/check_reference.py"
native_flags=(-std=c++11 -Wall -Wextra -Werror -O2)
if [ "$(uname -s)" = Darwin ]; then
  native_flags+=(-isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1")
fi
c++ "${native_flags[@]}" -I "$ECHO_ROOT/firmware/echo_game" -I "$ECHO_ROOT/.build" \
  "$ECHO_ROOT/tests/probe.cpp" "$ECHO_ROOT/tests/hotpath_checks.cpp" \
  "$ECHO_ROOT/firmware/echo_game/EchoEngine.cpp" \
  "$ECHO_ROOT/firmware/echo_game/EchoLevels.cpp" \
  "$ECHO_ROOT/firmware/echo_game/EchoApp.cpp" "$ECHO_ROOT/firmware/echo_game/Raster.cpp" \
  -o "$ECHO_ROOT/.build/probe"
python3 "$ECHO_ROOT/tests/check_reference.py" "$ECHO_ROOT/.build/probe"
"$ECHO_ROOT/.build/probe" ui
mkdir -p "$ECHO_ROOT/.build/preview"
"$ECHO_ROOT/.build/probe" preview "$ECHO_ROOT/.build/preview"
