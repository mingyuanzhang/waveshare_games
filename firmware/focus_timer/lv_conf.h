#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
// The Waveshare display driver swaps RGB565 bytes before sending them.
#define LV_COLOR_16_SWAP 0
#define LV_MEM_SIZE (64U * 1024U)
#define LV_TICK_CUSTOM 0
#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 20
#define LV_DRAW_COMPLEX 1
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#endif

