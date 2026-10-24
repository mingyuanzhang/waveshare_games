#!/bin/bash
set -euo pipefail

WAVESHARE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WAVESHARE_CLI="$WAVESHARE_ROOT/.tools/arduino-cli"
WAVESHARE_CONFIG="$WAVESHARE_ROOT/.arduino/config.json"
WAVESHARE_VENDOR="$WAVESHARE_ROOT/.vendor/board"
WAVESHARE_EXAMPLES="$WAVESHARE_VENDOR/Examples/Arduino-V3.2.0"
WAVESHARE_FQBN='esp32:esp32:esp32s3:FlashSize=16M,FlashMode=qio,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB'

waveshare_cli() {
  ARDUINO_BUILD_CACHE_PATH="$WAVESHARE_ROOT/.cache/build" \
    "$WAVESHARE_CLI" --config-file "$WAVESHARE_CONFIG" "$@"
}

require_setup() {
  if [ ! -x "$WAVESHARE_CLI" ] || [ ! -f "$WAVESHARE_CONFIG" ] || [ ! -f "$WAVESHARE_ROOT/.arduino/setup-complete" ]; then
    echo 'Run bash scripts/setup.sh first.' >&2
    exit 1
  fi
}

