#!/usr/bin/env python3
"""Stage local drivers, native game code, and the generated flash-only audio blob."""
import importlib.util,json,pathlib,shutil
ROOT=pathlib.Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('echo_stage',ROOT.parent/'echo_game/scripts/stage.py')
echo=importlib.util.module_from_spec(spec);spec.loader.exec_module(echo)
destination=ROOT/'.build/sketch/round_wonders'
if destination.exists():shutil.rmtree(destination)
destination.mkdir(parents=True)
for name in echo.DRIVERS:shutil.copy2(echo.EXAMPLE/name,destination/name)
for folder in [ROOT/'firmware/round_wonders',ROOT/'vendor/es8311']:
 for source in folder.iterdir():
  if source.suffix in ['.cpp','.c','.h','.ino','.csv']:shutil.copy2(source,destination/source.name)
for name in ['Raster.h','Raster.cpp','DisplaySync.h','DisplaySync.cpp']:shutil.copy2(ROOT.parent/'echo_game/firmware/echo_game'/name,destination/name)
assets=ROOT/'.build/assets'
for name in ['AudioAssets.h','audio.bin']:shutil.copy2(assets/name,destination/name)
display=destination/'Display_ST77916.cpp';text=display.read_text()
for old,new in [
 ('#include "Display_ST77916.h"','#include "Display_ST77916.h"\n#include "DisplaySync.h"'),
 ('.on_color_trans_done = NULL,','.on_color_trans_done = echo_lcd_color_done,'),
 ('uint8_t register_data[4];','uint8_t register_data[4] = {0};'),
 ('// panel_io_spi_del(io_handle);','ESP_ERROR_CHECK(esp_lcd_panel_io_del(io_handle));'),
 ('  test_draw_bitmap(panel_handle);','  // The round world presents the first frame.'),
 ('esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, color);','ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, Xstart, Ystart, Xend, Yend, color));\n  echo_lcd_wait();')]:text=echo.replace_once(text,old,new)
display.write_text(text)
header=destination/'Display_ST77916.h';text=header.read_text().replace('#include "Touch_CST816.h"','#include "Touch_CST816.h"\n#include "DisplaySync.h"').replace('(2048)','(EXAMPLE_LCD_WIDTH * kEchoStripRows * sizeof(uint16_t))');header.write_text(text)
blob=json.dumps(str(destination/'audio.bin'))
(destination/'AudioData.S').write_text('.section .rodata.wonder_audio,"a",@progbits\n.balign 4\n.global WonderAudioBlob\nWonderAudioBlob:\n.incbin '+blob+'\n')
print(destination)
