#!/usr/bin/env python3
"""Stage only this experiment and the official LCD/touch drivers."""
import pathlib
import shutil

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXAMPLE = ROOT.parent / ".vendor/board/Examples/Arduino-V3.2.0/examples/01_lvgl_demo"
DRIVERS = (
    "Display_ST77916.cpp", "Display_ST77916.h", "Touch_CST816.cpp", "Touch_CST816.h",
    "I2C_Driver.cpp", "I2C_Driver.h", "esp_lcd_st77916.c", "esp_lcd_st77916.h",
)


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("The vendor source changed; review this patch: " + old)
    return text.replace(old, new, 1)


def stage():
    if not all((EXAMPLE / name).is_file() for name in DRIVERS):
        raise RuntimeError("Run bash scripts/setup.sh from the parent waveshare folder first.")
    destination = ROOT / ".build/sketch/echo_game"
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for name in DRIVERS:
        shutil.copy2(EXAMPLE / name, destination / name)
    for source in (ROOT / "firmware/echo_game").iterdir():
        if source.suffix in (".c", ".cpp", ".h", ".ino"):
            shutil.copy2(source, destination / source.name)

    display = destination / "Display_ST77916.cpp"
    text = display.read_text()
    patches = (
        ('#include "Display_ST77916.h"', '#include "Display_ST77916.h"\n#include "DisplaySync.h"'),
        ('.on_color_trans_done = NULL,', '.on_color_trans_done = echo_lcd_color_done,'),
        ('uint8_t register_data[4];', 'uint8_t register_data[4] = {0};'),
        ('// panel_io_spi_del(io_handle);', 'ESP_ERROR_CHECK(esp_lcd_panel_io_del(io_handle));'),
        ('  test_draw_bitmap(panel_handle);', '  // Echo presents its first frame after initialization.'),
        ('esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, color);',
         'ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, color));\n'
         '  echo_lcd_wait();'),
    )
    for old, new in patches:
        text = replace_once(text, old, new)
    display.write_text(text)
    display_header = destination / "Display_ST77916.h"
    text = display_header.read_text()
    text = replace_once(text, '#include "Touch_CST816.h"',
                        '#include "Touch_CST816.h"\n#include "DisplaySync.h"')
    text = replace_once(text, '(2048)', '(EXAMPLE_LCD_WIDTH * kEchoStripRows * sizeof(uint16_t))')
    display_header.write_text(text)
    return destination


if __name__ == "__main__":
    try:
        print(stage())
    except RuntimeError as error:
        raise SystemExit(str(error))
