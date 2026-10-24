#!/bin/bash
# Downloads tools and board sources locally; does not flash the device.
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ "$(uname -s)" != Darwin ]; then
  echo 'This setup script currently supports macOS.' >&2
  exit 1
fi

case "$(uname -m)" in
  arm64)
    cli_archive='arduino-cli_1.3.1_macOS_ARM64.tar.gz'
    cli_sha256='723f8ddaf4875e87a20380c88c15ff9d55916c3c80cb432f1f980ea32cec2341'
    ;;
  x86_64)
    cli_archive='arduino-cli_1.3.1_macOS_64bit.tar.gz'
    cli_sha256='a0ca73f7e599d33a532fb2b1f8af9260541d990a684988bf8208abb42be83ced'
    ;;
  *) echo 'Unsupported Mac architecture.' >&2; exit 1 ;;
esac

mkdir -p "$WAVESHARE_ROOT/.tools" "$WAVESHARE_ROOT/.cache" "$WAVESHARE_ROOT/.arduino" "$WAVESHARE_ROOT/.vendor"

if [ ! -x "$WAVESHARE_CLI" ]; then
  echo 'Downloading Arduino CLI 1.3.1...'
  curl --fail --location --retry 3 --connect-timeout 20 \
    "https://github.com/arduino/arduino-cli/releases/download/v1.3.1/$cli_archive" \
    --output "$WAVESHARE_ROOT/.cache/$cli_archive"
  actual_sha256="$(shasum -a 256 "$WAVESHARE_ROOT/.cache/$cli_archive" | awk '{print $1}')"
  if [ "$actual_sha256" != "$cli_sha256" ]; then
    echo 'Arduino CLI archive checksum did not match the official release.' >&2
    exit 1
  fi
  tar -xzf "$WAVESHARE_ROOT/.cache/$cli_archive" -C "$WAVESHARE_ROOT/.tools" arduino-cli
fi

python3 - "$WAVESHARE_ROOT" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
config = {
    "board_manager": {
        "additional_urls": ["https://espressif.github.io/arduino-esp32/package_esp32_index.json"]
    },
    "directories": {
        "data": str(root / ".arduino/data"),
        "downloads": str(root / ".cache/packages"),
        "user": str(root / ".arduino/user"),
    },
    "network": {"connection_timeout": "30m"},
    "build_cache": {"path": str(root / ".cache/build")},
    "updater": {"enable_notification": False},
}
(root / ".arduino/config.json").write_text(json.dumps(config, indent=2) + "\n")
PY

echo 'Installing ESP32 Arduino core 3.2.0 (the first download can take several minutes)...'
waveshare_cli core update-index
installed=0
for attempt in 1 2 3; do
  if waveshare_cli core install esp32:esp32@3.2.0; then
    installed=1
    break
  fi
  echo "ESP32 download attempt $attempt failed; retrying available packages..." >&2
done
if [ "$installed" != 1 ]; then
  echo 'ESP32 installation failed after three attempts. Rerun this script to retry.' >&2
  exit 1
fi

if [ ! -d "$WAVESHARE_VENDOR/.git" ]; then
  echo 'Downloading the official Waveshare display example and LVGL library...'
  git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B.git "$WAVESHARE_VENDOR"
fi
git -C "$WAVESHARE_VENDOR" sparse-checkout set \
  Examples/Arduino-V3.2.0/examples/01_lvgl_demo \
  Examples/Arduino-V3.2.0/libraries/lvgl
git -C "$WAVESHARE_VENDOR" rev-parse HEAD > "$WAVESHARE_ROOT/.arduino/vendor-commit.txt"

python3 - "$WAVESHARE_ROOT" <<'PY'
import pathlib
import shutil
import sys

root = pathlib.Path(sys.argv[1])
library_root = root / ".arduino/user/libraries"
library_root.mkdir(parents=True, exist_ok=True)
vendor_lvgl = root / ".vendor/board/Examples/Arduino-V3.2.0/libraries/lvgl"
if not (vendor_lvgl / "src/lvgl.h").is_file():
    raise SystemExit("The vendor LVGL library is missing from the downloaded package.")
shutil.copytree(vendor_lvgl, library_root / "lvgl", dirs_exist_ok=True)
print("Official board sources and LVGL are ready.")
PY

touch "$WAVESHARE_ROOT/.arduino/setup-complete"
waveshare_cli version
waveshare_cli board list
echo 'Setup complete. No firmware has been uploaded.'
