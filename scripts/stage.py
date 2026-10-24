#!/usr/bin/env python3
"""Assemble a sketch using the official board drivers without editing the vendor tree."""

import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXAMPLE = ROOT / ".vendor/board/Examples/Arduino-V3.2.0/examples/01_lvgl_demo"


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("The vendor display source changed; review this patch: " + old)
    return text.replace(old, new, 1)


def stage(target):
    if target not in ("demo", "timer", "timer-test"):
        raise ValueError("Target must be demo, timer, or timer-test.")
    if not (EXAMPLE / "01_lvgl_demo.ino").is_file():
        raise RuntimeError("Official board sources are missing. Run bash scripts/setup.sh.")
    sketch_name = "01_lvgl_demo" if target == "demo" else "focus_timer"
    destination = ROOT / ".build/sketches" / target / sketch_name
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)

    for source in EXAMPLE.iterdir():
        if source.suffix in (".c", ".cpp", ".h", ".ino"):
            if target != "demo" and source.suffix == ".ino":
                continue
            shutil.copy2(source, destination / source.name)

    if target != "demo":
        for source in (ROOT / "firmware/focus_timer").iterdir():
            if source.is_file():
                shutil.copy2(source, destination / source.name)

        display = destination / "Display_ST77916.cpp"
        text = display.read_text()
        text = replace_once(text, '#include "Display_ST77916.h"',
                            '#include "Display_ST77916.h"\n#include "DisplaySync.h"')
        text = replace_once(text, '.on_color_trans_done = NULL,',
                            '.on_color_trans_done = focus_lcd_color_done,')
        text = replace_once(text, 'uint8_t register_data[4];',
                            'uint8_t register_data[4] = {0};')
        text = replace_once(text, '// panel_io_spi_del(io_handle);',
                            'ESP_ERROR_CHECK(esp_lcd_panel_io_del(io_handle));')
        # The vendor's startup color test frees its buffer before DMA finishes.
        # The timer draws its own first frame after registering LVGL instead.
        text = replace_once(text, '  test_draw_bitmap(panel_handle);',
                            '  // The focus timer draws the first frame through LVGL.')
        text = replace_once(
            text,
            'esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, color);',
            'ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, color));\n'
            '  focus_lcd_wait();',
        )
        display.write_text(text)

        lvgl = destination / "LVGL_Driver.cpp"
        text = lvgl.read_text()
        text = replace_once(text, 'disp_drv.full_refresh = 1;', 'disp_drv.full_refresh = 0;')
        lvgl.write_text(text)

    # Library compilation and sketch compilation must use the same LVGL config.
    libraries = ROOT / ".arduino/user/libraries"
    libraries.mkdir(parents=True, exist_ok=True)
    shutil.copy2(destination / "lv_conf.h", libraries / "lv_conf.h")
    return destination


if __name__ == "__main__":
    try:
        print(stage(sys.argv[1] if len(sys.argv) > 1 else "timer"))
    except (ValueError, RuntimeError) as error:
        raise SystemExit(str(error))

