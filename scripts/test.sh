#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
mkdir -p "$WAVESHARE_ROOT/.build/tests"
c++ -std=c++11 -Wall -Wextra -Werror \
  "$WAVESHARE_ROOT/tests/countdown_test.cpp" \
  -o "$WAVESHARE_ROOT/.build/tests/countdown_test"
"$WAVESHARE_ROOT/.build/tests/countdown_test"
