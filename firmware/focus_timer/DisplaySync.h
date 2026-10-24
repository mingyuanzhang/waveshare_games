#pragma once

#include "esp_attr.h"
#include "esp_lcd_panel_io.h"

void focus_lcd_sync_init();
void focus_lcd_wait();
bool IRAM_ATTR focus_lcd_color_done(esp_lcd_panel_io_handle_t io,
                                  esp_lcd_panel_io_event_data_t* event,
                                  void* context);

