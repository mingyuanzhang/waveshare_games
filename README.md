# Waveshare ESP32-S3-Touch-LCD-1.85B experiments

The first prototype is a touch focus timer with 5, 25, and 50 minute presets, a progress ring, start/pause/resume controls, reset, and a visual completion alert. The source is in `firmware/focus_timer`. Clock synchronization and sound are later additions.

A separate [Echo game experiment](echo_game/README.md) lives in `echo_game/`, with its own firmware, build scripts, and tests. It includes all 25 rooms from the browser game, a paged room selector, directional visual echoes, touch aiming, movement through the physical BOOT button, and saved best scores. It shares the installed tools and flash backups with the timer. Its `scripts/check-speed.sh` uploads the game and runs a responsiveness check on the board.

[Round Wonders](team_lab/README.md) adapts TeamLab 2D into a radial world with tiny touch-placed geometry and all six Wonder Audio music styles. It runs from internal flash without a microSD card. BOOT clicks select shape and music, leaving the screen for the people and discoveries. Its installer backs up Echo before replacement and includes an Echo restoration command.

## Run the prototype

Connect the **ESP32-S3-Touch-LCD-1.85B** to the Mac with a USB-C data cable. Run setup from this folder:

```sh
bash scripts/setup.sh
```

Setup downloads Arduino CLI 1.3.1, ESP32 core 3.2.0, and the official board drivers and LVGL library into this folder. The first download can take several minutes. It allows 30 minutes per request and retries package installation. Rerunning setup retains completed downloads. Setup does not upload firmware.

First verify the display and touch with the official demo. Replace the port below if the board appears under a different name:

```sh
bash scripts/run.sh demo /dev/cu.usbmodem1101
```

The upload script builds first, identifies the ESP32-S3, and saves its original 16 MB flash in `backups/` before uploading. Then install the timer:

```sh
bash scripts/run.sh timer /dev/cu.usbmodem1101
```

Choose a preset, tap Start, and use Pause/Resume or Reset. Presets are disabled during a session; Reset enables them again. At completion, the countdown turns amber and displays "Time for a break". Tap Again to restart the selected duration. Sessions reset when the board restarts.

For a quick hardware check, use 5, 25, and 50 **second** sessions:

```sh
bash scripts/run.sh timer-test /dev/cu.usbmodem1101
```

Verify that a session starts, remains unchanged while paused, resumes correctly, resets, and reaches the completion screen. Install `timer` again afterward for normal minute presets.

To compile without uploading, use `bash scripts/build.sh timer`. To read device logs, use `bash scripts/monitor.sh /dev/cu.usbmodem1101`; close the monitor before another upload.

To return to the firmware that was present before the first upload, run `bash scripts/restore.sh /dev/cu.usbmodem1101`. This restores the matching device's original flash backup after checking its size and checksum.

If upload cannot connect, power the board off, hold BOOT while powering it on, release BOOT, and check its serial port again. A board flashed in manual download mode may need a power cycle afterward. [Waveshare troubleshooting](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B/FAQ).

## Development checks

The official display demo, normal timer, and quick-test timer compile successfully with ESP32 core 3.2.0 and LVGL 8.4.0. The countdown logic tests pass. On October 14, 2026, the 5-second test was confirmed on the actual device: the display, preset selection, Start control, and completion screen worked. The original 16 MB flash backup is saved and its checksum has been verified.

Run the timer logic tests with `bash scripts/test.sh`. They cover sub-second pause/resume accuracy, long pauses, delayed completion, a Pause tap arriving at completion, resetting, preset changes, and uptime beyond the 32-bit millisecond range.

The sketch staging script copies the official drivers into `.build`, keeping vendor sources intact. For the timer, it waits for display DMA before releasing an LVGL draw buffer, removes the vendor startup color test, releases the display's temporary probe interface, and enables partial redraws. The board initialization commands and touch coordinates come from the official example.

Hidden tooling folders, downloaded board sources, caches, build output, generated audio/previews, runtime data, and flash backups are ignored by Git. Firmware, scripts, tests, configuration, documentation, and the ES8311 driver and license are tracked. The exact downloaded board commit is recorded locally in `.arduino/vendor-commit.txt`.

Research date: October 14, 2026. These notes apply to the **1.85B** model.

## Device capabilities

| Hardware | Specification |
| --- | --- |
| Processor | ESP32-S3R8, dual core, up to 240 MHz |
| Memory | 16 MB flash, 8 MB PSRAM |
| Display | 1.85-inch capacitive touch LCD, 360 × 360 pixels |
| Display and touch drivers | ST77916 over QSPI; CST816S over I2C |
| Wireless | 2.4 GHz Wi-Fi and Bluetooth LE |
| Motion | QMI8658 accelerometer and gyroscope |
| Clock | PCF85063 real-time clock |
| Audio | Dual microphones, ES7210 input circuitry, ES8311 codec, speaker connection |
| Storage and power | MicroSD, USB-C, lithium battery charging and battery gauge |

Specifications: [Waveshare board documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B).

Bluetooth Classic is unavailable on the ESP32-S3, so a conventional Bluetooth audio speaker is a poor fit. [Espressif Bluetooth documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/ble/overview.html).

## Project ideas

These are proposed uses of the hardware. Difficulty is relative to a first embedded project.

| Project | What it would do | Difficulty |
| --- | --- | --- |
| Focus timer and clock | Touch presets, countdown, break reminders, clock view | Low |
| Weather clock | Show time and a compact forecast fetched over Wi-Fi | Medium; needs a weather service |
| Desk status display | Show a personal reminder, computer status, or task count | Medium; needs a source of data |
| Smart-home remote | Touch buttons for lights and scenes | Medium; needs a compatible home system |
| Tilt game | Move a ball using the motion sensor | Low to medium |
| Voice assistant | Capture speech and play spoken replies | Higher; conversational AI needs a remote service |

The board also has an official offline wake-word and command-recognition example, which could later support simple timer commands. [Waveshare ESP-IDF examples](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B/ESP-IDF#04_esp_wakeword_det).

## Later additions

A clock view synchronized over Wi-Fi, focus and break cycles, brightness settings, saved presets, and a completion chime can follow the first hardware test. The countdown uses a monotonic clock, so changing the wall clock will not affect session timing.

## Development approach

Start with **Arduino C++ and LVGL**, the graphical interface library used by the vendor's display example. Waveshare specifies ESP32 Arduino core **3.2.0 or later**, **LVGL 8.4.0**, and **SensorLib 0.3.1**. Use the supplied library bundle initially; LVGL 8 and 9 have incompatible APIs. [Waveshare Arduino guide](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B/Arduino).

The official repository contains [Arduino examples and libraries for core 3.2.0](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/tree/main/Examples/Arduino-V3.2.0). The relevant starting examples are `01_lvgl_demo` for the interface and `08_I2C_pcf85063` for the clock.

ESP-IDF is another supported path if the project grows to need more control over audio, tasks, or power. The vendor currently specifies **ESP-IDF 5.5.3 or later**. [Waveshare ESP-IDF guide](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B/ESP-IDF).

The vendor supplies a factory restoration image, `ESP32-S3-Touch-LCD-1.85B-Factory.bin`, in its repository's `Firmware` directory. [Factory firmware instructions](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B/Instructions-For-Use).
