# Round Wonders

A native adaptation of TeamLab 2D for **Waveshare ESP32-S3-Touch-LCD-1.85B**, using its 360 × 360 round screen and ES8311 speaker output. Everything runs from internal flash and PSRAM. No microSD, network, browser, or audio download is used.

## Play

The screen belongs to the little people and their discoveries. There is no permanent toolbar.

Use the **action button marked BOOT** for the game controls. Hold it for **one second** to turn all music and discovery sounds off or on. A brief startup hint reminds you of this gesture. PWR controls the board's power.

| Input | Action |
| --- | --- |
| Action / BOOT, one click | Next shape |
| Action / BOOT, two clicks within 300 ms | Next music style |
| Action / BOOT, hold one second | Sound off or on |
| Power off, then on | Reset the whole game |
| Touch a free spot | Place the selected tiny shape |
| Drag an existing shape | Move it |

Single clicks wait for the double-click interval; touching the screen commits a pending single click immediately. A brief label confirms shape, music, or mute changes. Holding BOOT once toggles all sound and briefly shows `SOUND OFF` or `SOUND ON`; release it before holding again to toggle back. Music fades smoothly and keeps its place while muted. The scene keeps running, and holding longer has no additional action.

PWR retains its hardware power function. Powering off and on removes every placed shape and effect, restarts the people, and returns to the circle shape, Default music, and sound on. There are no flash writes during play. Six copies of each shape are available; placing a seventh moves the oldest copy, so no delete control is necessary. Placement avoids people and the extreme rim.

The outer rim is ground and the center is high. Twenty-four people walk around the rim and curved platforms with their heads toward the center, climb radial ladders, step off or turn at edges, and bounce inward from a spring. Angles wrap continuously around the circle. Gravity points outward. Rain falls radially toward the closest ground and makes rim ripples. Fireworks, balloons, ribbons, and orbits rise inward and dissolve as they reach the center, without crossing through it. Gardens grow inward from their placement. Six small shapes reveal these effects on independent timers, with a spring overshoot at the entrance.

## Install and restore Echo

From the `games_exp` workspace root, with the existing Waveshare tools installed:

```sh
bash waveshare/team_lab/scripts/run.sh
```

The installer automatically selects the USB serial port when exactly one is available. Connect the powered-on board with a USB data cable. If no port appears, reconnect USB and check the cable. If several ports appear, pass the board's path explicitly, for example `bash waveshare/team_lab/scripts/run.sh /dev/cu.usbmodem1101`. A supplied path that has disappeared reports the available ports instead of selecting a different device.

This runs native checks, builds the firmware, identifies the connected board, and saves **both existing Echo application slots, boot information, and settings** before uploading. The snapshot is `waveshare/backups/<mac>-before-round-wonders.bin`, 6,356,992 bytes, with a SHA-256 sidecar. Upload stops if its size or checksum is wrong. The remaining data partition is untouched. Backups are local, ignored files: retain them separately if moving workspaces.

Restore the saved Echo firmware and settings:

```sh
bash waveshare/team_lab/scripts/restore-echo.sh
```

If there is no matching snapshot, restoration rebuilds [Echo's preserved source](../echo_game/README.md). Echo's source and its own build/run scripts are unchanged. A copy of its previously built binaries is also retained locally under `.build/echo-restore/`. The older full 16 MiB factory snapshot in `waveshare/backups/` remains available through the parent restoration script.

Use the actual USB port if it changes. Close serial monitors before upload. A board put manually into download mode may need a power cycle after flashing. The firmware needs the **1.85B** model's pinout.

## Music and memory

[Wonder Audio's exporter](../../wonder-audio/scripts/export-embedded.cjs) renders the existing six accompaniments and 126 reusable instrument samples at build time, compressing them into independent IMA ADPCM blocks. The binary also contains 4,752 cue variants covering every harmonic entrance phase, with 53,396 scored notes. These are generated from the same harmony, swing, expression, and discovery gestures as the browser player.

At runtime the accompaniment streams from memory-mapped flash. The active instrument bank is decoded into PSRAM; a second bank lets music continue while the next style loads. Decoding is divided into small batches with a roughly 5 ms work budget per loop iteration, followed by a short fade between styles. Its exact loading time requires a device measurement. The sampler interpolates pitches, preserves velocity choices and gates, adds a small shared room, and limits peaks. A discovery gets its full melody whenever one of the three foreground slots is free. Only actual contenders on the same entrance can reassign a pending reservation, favoring copies that have waited longest, including copies of the same shape. Melodies already playing finish uninterrupted. Other discoveries give short accents at about half the opening note's amplitude, with up to 320 ms of scored note duration and a short release. Simultaneous accents of the same shape share a sound. Foreground turns remain shared fairly among all 36 shapes.

Audio has its own core-0 task and a three-block I2S DMA buffer. Visual entrances use the played-sample clock and the same swung entrance grid as the music. Discoveries deliberately wait for a musical boundary: typically up to an eighth note, or a dotted-quarter pulse in Chopin, plus a small scheduling lead. This rhythmic wait is separate from touch responsiveness. The screen targets 25 fps; fixed 60 Hz physics keeps movement consistent. At most five effects receive full line detail at once, with older effects becoming faint traces.

| Resource | Built or allocated size |
| --- | --- |
| Flash audio and scores | 5,487,652 bytes (5.23 MiB) |
| Application partition | 6 MiB, merged from Echo's two 3 MiB slots |
| Firmware, including audio | About 5.69 MiB; see current build output |
| Largest decoded bank | 2,488,320 bytes (2.37 MiB) |
| Two reserved banks | 4,976,640 bytes |
| Two RGB565 frame buffers in PSRAM | 518,400 bytes |
| Internal DMA display strip | 28,800 bytes |
| Speaker stream | 24 kHz, 16-bit mono |

The original FAT partition remains at `0x610000`, but the game never mounts it. This uses a factory application partition and USB flashing; OTA is not implemented. Mono compression, sampled pitch shifts, and a smaller room make the sound different from the browser version. Rock's noisier backing has the lowest measured ADPCM signal-to-noise ratio, about 28.6 dB; other backing loops measure about 37.8–46.5 dB. Individual short samples can measure lower. These numerical checks establish signal integrity; speaker loudness, tonal balance, distortion, and perceived quality still need listening on the board. The piano score and driver notices ship with the project.

## Checks and previews

```sh
bash waveshare/team_lab/scripts/test.sh
bash waveshare/team_lab/scripts/build.sh
python3 waveshare/team_lab/scripts/check_speed.py /dev/cu.usbmodem1101
```

Native checks exercise click/double-click/hold handling, radial physics and wrapping, ladders and ledges, ten simulated minutes of bounded movement, the 36-shape inventory, outward/inward geometry, framebuffer guards, matching JS/C++ ADPCM decoding, all six music styles, mute fades, stale-cue rejection, smooth bank transitions, free-slot melody playback, same-beat competition without interrupting audible phrases, accent signal levels, and foreground fairness across 36 copies. Preview generation uses Python with Pillow; the exporter uses Node with no npm dependencies. Output includes PNGs and six five-second WAV clips under `.build/preview/`; `discoveries.png` shows the actual native renderer.

The device benchmark runs a crowded scene for 45 seconds and cycles the six music styles, then restores the user's scene. It captures average/max/p95 frame rendering plus LCD transfer time, effective frame rate, frames over 40 ms, mixer time, underruns, late cues, and remaining memory in `.build/benchmark-device.*`. These figures include real LCD transfers only when run on the board. The earlier Echo benchmark does not establish this game's frame rate.

As of October 23, 2026, native checks and the ESP32-S3 build have passed. Installation and physical performance/audio checks are pending: this Codex session sees `/dev/cu.usbmodem1101`, but opening it returns `Operation not permitted`. Echo has not been overwritten from this session.

Serial commands are `S` for shape, `T` for music, `M` for mute, `?` for statistics, and `B` for the benchmark. Use `bash waveshare/team_lab/scripts/monitor.sh /dev/cu.usbmodem1101` to inspect boot diagnostics, including codec acknowledgement, audio readiness, and memory.

## Source layout

`RoundWorld` and `RoundRenderer` implement the radial simulation and artwork. `Music` is the hardware-independent sampler and conductor; `DeviceAudio` owns the codec, I2S, audio task, and background bank loading. `Controls` handles the physical button. `RoundDevice` connects touch, physics, audio-clock cues, and display transfer. Staging reuses Echo's tested rasterizer and DMA synchronization plus the installed official display/touch drivers. The pinned ES8311 driver and its license are in `vendor/es8311/`.

The browser games retain their existing rectangular worlds. Their generated Wonder Audio distributions include the new instrument-export API; their gameplay is unchanged.
