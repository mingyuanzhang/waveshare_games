# Echo on the Waveshare ESP32-S3 Touch-LCD-1.85B

A standalone port of all 25 rooms from the browser game in `../../echo`. Find the exit by sending directional calls and reading the returning light. The room stays hidden until you escape or choose Reveal Room.

This experiment has its own firmware, scripts, tests, and `.build` directory. It shares the installed Arduino tools, board drivers, and flash backups in the parent `waveshare` folder. Other experiments can sit alongside `echo_game`.

## Install

The existing Waveshare setup is sufficient. From this directory, with the board connected:

```sh
bash scripts/run.sh /dev/cu.usbmodem101
```

The script compiles the game, verifies the connected device's existing flash backup (or creates one for a new device), and uploads Echo. Replace the port if it changes. The board runs one application at a time; uploading Echo replaces the running timer. Both projects' source and build files remain available.

To compile without uploading:

```sh
bash scripts/build.sh
```

To reinstall the timer, run this from the parent `waveshare` directory:

```sh
bash scripts/run.sh timer /dev/cu.usbmodem101
```

The parent `scripts/restore.sh` restores the original full-flash backup.

## Play

- Tap **PLAY**, then tap a direction around the blue dot at the center of the display to send a call and face that direction. Returning marks show nearby walls; look for a silent opening.
- Press the physical **BOOT** button to move forward one step. A red heading means a wall blocked the step. Holding the button does not repeat steps.
- Tap **ECHO MENU** at the top for Continue, Restart, Rooms, How to Play, or Reveal Room. The menu also shows the battery gauge's charge and voltage reading. Rooms shows three entries per page; use Prev and Next to reach all nine pages. It opens on the page containing your current room.
- Escape to see the room, your route, and your score. Tap Next to advance.

Each screen tap triggers once, when your finger lifts; a physical step triggers after a debounced press. STEP advances three world units, stopping before a wall. There is no TURN control: direction taps always send a call. You can aim by tapping close to the blue dot instead of reaching the display edge. Scores follow the browser game: 1,000 minus penalties for calls and moves beyond the room's par, with a floor of 60 for an escape. Revealing a room scores zero. Your selected room and best scores survive a restart; your current route starts over.

All 25 rooms are included, from **One Door** through **The Last Door**, with the original geometry, materials, order, and par scores. Existing best scores for the first three rooms carry over. This version uses visual echoes; audio and motion controls are possible later additions.

## Verification

The user confirmed that the physical BOOT button works for STEP and that the centered circle layout works well on the device. The echo circle stays at (180, 180) on the 360 × 360 display, with the menu near the top. The optimized all-level build uses 470,966 bytes (about 460 KiB) of the 3 MiB application slot and 68,820 bytes of static RAM. It compiles with ESP32 Arduino core 3.2.0, `-O2`, and the board's 16 MB flash / 8 MB OPI PSRAM settings.

Run the native comparison against the original TypeScript game:

```sh
bash scripts/test.sh
```

The test requires the original `echo/node_modules` installation, Node, Python 3, and a C++ compiler. It compares 63,073 echo records (including all four materials), 1,200 movement transitions, and all 25 perfect-score escapes against the original game. The winning routes come from the original game's solver and are replayed through both implementations. Solver results are cached with a fingerprint of their geometry, rules, and solver source.

The checks also cover physical steps, button bounce and long holds, startup with a held button, touch actions, selection of every room, paging limits, the final page's empty rows, saved-score requests, a bounded 512-position route history, and framebuffer boundaries while rendering screen previews. Partial animation updates are compared pixel-for-pixel against full redraws at five animation ages in every room. Another 36,388 ray comparisons cover random origins, corners, almost-parallel directions, and scenes exceeding the wall-cache capacity. The drawing checks compare 400 clipped lines and rings against a full-viewport reference. All 11 saved preview screens remained pixel-identical after the performance changes.

`scripts/sync-game.sh` exports all rooms from the browser source into `firmware/echo_game/EchoLevels.h` and `EchoLevels.cpp`. The generated snapshot keeps a single copy of the level tables; normal device builds need neither Node nor the browser project. Tests refresh the snapshot and original-game reference data without editing the browser project.

The test writes actual renderer frames to `.build/preview`. Optionally, with Pillow installed, run `python3 scripts/preview.py` to make PNGs and a round-screen contact sheet at `.build/preview/screens.png`.

Read device logs with:

```sh
bash scripts/monitor.sh /dev/cu.usbmodem101
```

Close the monitor before uploading again. Logs report ray calculation time, average drawing plus display-transfer time during animations, and battery voltage, charge, current, and raw status flags every five seconds.

## Responsiveness check

To upload the regular 25-room game and measure it on the board, run this in a regular Mac Terminal:

```sh
bash scripts/check-speed.sh /dev/cu.usbmodem101
```

The check cycles through all rooms automatically, sampling 24 direction calls per room at the initial position and after walking up to three steps. Each call is rendered at four ages (80, 300, 700, and 1,200 ms), for a total of 600 calls and 2,400 frames. The first frame after a call is a full redraw; its three following animation frames update the echo field, matching the normal game's rendering path. Calls include the full ray calculation and preparation of the animation. Frame times on the ESP32 include drawing, copying from PSRAM, byte swapping, and completing LCD DMA transfers. The summary also separates rendering from copying and display transfer. Frames are presented without the usual frame-rate delay so that their measured cost can be compared with the 40 ms budget for 25 fps.

The benchmark temporarily saves the live game in PSRAM and restores it afterward. It does not save benchmark scores or room selections to flash. The initial firmware upload restarts the current route as usual. Best scores remain stored. Progress and a summary print in Terminal, and results are saved to `.build/benchmark-device.txt` and `.build/benchmark-device.json`. The prior successful JSON report is preserved as `.build/benchmark-device.previous.json`, and its maximum call/frame times are printed for comparison. The program stays installed as the regular game afterward.

To repeat the timing check without uploading again:

```sh
python3 scripts/check_speed.py /dev/cu.usbmodem101
```

Close other serial monitors first. If the port changes, use the current USB serial port. To summarize an existing transcript, use `python3 scripts/check_speed.py --report .build/benchmark-device.txt`.

For a native CPU comparison:

```sh
bash scripts/benchmark-native.sh
```

The first ESP32 measurement completed all 600 calls and 2,400 frames. Its slowest call took **262.99 ms** in The Last Door, and its slowest frame took **126.00 ms** in The Landing. All 2,400 sampled frames exceeded 40 ms. That baseline used 45 small transfers and a full redraw for every frame; it showed that storage capacity was sufficient but responsiveness needed work. The original device report and screen previews were saved in `.build/before-optimization/`.

The first optimization reduced the slowest sampled call to **154.78 ms** and the slowest frame to **64.92 ms**, improvements of about 41% and 48%. Of 2,400 frames, **687 exceeded 40 ms**; 71.4% met the 25 fps budget. Rendering peaked at **46.72 ms**, while copying and display transfer peaked at **19.17 ms**. Those maxima can come from different frames. That report and the corresponding source and previews are saved in `.build/after-first-optimization/`.

The firmware reduces display transfers to nine per full redraw and six per animation update, caches fixed beam weights at startup, rejects clearly invalid ray intersections before division, and sorts small indices before moving echo records. Geometry and scoring remain in double precision; pixel-only brightness and direction calculations use single precision. The ESP32-S3's hardware FPU supports single precision, while double arithmetic uses software. [Espressif FPU reference](https://developer.espressif.com/blog/2025/10/cores_with_fpu/)

A second optimization narrows each drawing row to the visible stroke or ring, preserving the same coverage and blending. It also caches approximate wall endpoints for a conservative single-precision rejection of obvious ray misses. Possible intersections, including cases close to endpoints or parallel walls, continue through the original double-precision calculation. Rooms exceeding the 128-wall cache use the exact calculation without filtering. All 200 rays and both reflections remain enabled.

The second optimization completed all 25 rooms, 600 calls, and 2,400 frames on the ESP32. The slowest sampled call took **56.62 ms** in The Last Door; the slowest frame took **60.21 ms** in The Landing. **347 frames exceeded 40 ms**, so **85.5% met the 25 fps budget**. Maximum rendering time was **41.27 ms**, and maximum copying plus display transfer was **19.16 ms**. Compared with the initial baseline, maximum sampled call time fell by about 78% and maximum frame time by about 52%. The user confirmed that the game looks very good at this point; further tuning is deferred. The device reports, matching source, and previews are preserved in `.build/after-second-optimization/`.

All game checks and the native benchmark workload pass. Mac timings exclude LCD transfer and do not establish device responsiveness. The serial command, old/new result formats, streaming reader, and completion checks were verified with a simulated serial device; those simulated measurements are kept separately from real device results. The samples cover two positions and 24 directions per room, not every possible gameplay state. The game targets 25 fps; the measured slowest frames still exceed that budget.

## Battery power and the two buttons

The **PWR** button controls the board's power circuit. The **BOOT** button connects to GPIO0 (active low); Echo uses it as STEP after the application starts. Release BOOT before powering on or resetting, because holding it during startup selects the ESP32 download mode. [Waveshare board documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B)

For battery startup, unplug USB, leave BOOT released, and press PWR for about one second, then release. The board documentation describes about two seconds for power-off. Waveshare also documents a case where a PWR shutdown while USB is connected only takes effect once USB is unplugged; press PWR again to restore battery operation. [Waveshare power FAQ](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B/FAQ)

After the initial battery-startup report, the user confirmed that the PWR button works. No firmware power-enable workaround has been added. The revised firmware reads the onboard BQ27220 gauge without changing its configuration, and displays its charge and voltage readings in the menu. It does not wait for a USB serial connection to start. Charge percentage is the gauge's estimate; voltage provides an additional measurement. [TI BQ27220 command reference](https://www.ti.com/lit/pdf/sluubd4)

## Implementation

- `EchoEngine` ports the original geometry, 200-ray fan, two reflections, intensity calculation, collisions, and scoring to C++. Fixed beam weights are prepared once. A bounded wall cache filters obvious misses with conservative error margins, and actual intersections retain the original precision.
- `EchoLevels.h` declares the room count and shared level table; `EchoLevels.cpp` holds the exported geometry and par values.
- `EchoApp` handles the screens, direction taps, one active call, and echo timing. A new call replaces the previous call's animation; stepping clears it.
- `Raster` draws into one 360 × 360 RGB565 framebuffer in PSRAM (259,200 bytes), using a small built-in font. Opaque rectangles use row fills; thin lines and rings visit narrow row spans before applying the original pixel-coverage calculation.
- `EchoDevice` polls the CST816 touch controller, reads BOOT/GPIO0 through the debounced `ActionButton`, reads the BQ27220 battery gauge, saves changed scores and room selections with Preferences, and presents frames through a 40-row internal DMA strip buffer (28,800 bytes). Animation-only updates redraw and send the 300 × 240 echo area. Animations are capped at 25 frames per second; actual performance is measured on the board. Full redraws occur after an action or a changed battery reading in the menu.
- `Benchmark` runs the same call and frame workload on the Mac and ESP32. A developer serial command (`B`) starts the on-device run; `check_speed.py` sends it and collects the measurements. The normal user interface has no timing controls or performance overlays.
- `stage.py` copies the official ST77916 and CST816 drivers into this project's build directory. It applies the display synchronization fixes used by the working timer and leaves the downloaded vendor source intact. Echo does not use LVGL.

Board drivers come from the installed [official Waveshare repository](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B); its pinned revision is recorded in `../.arduino/vendor-commit.txt`.
