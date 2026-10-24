#pragma once

#include "esp_attr.h"
#include "esp_lcd_panel_io.h"

constexpr int kEchoStripRows = 40;

void echo_lcd_sync_init();
void echo_lcd_wait();
bool IRAM_ATTR echo_lcd_color_done(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_io_event_data_t* event,
                                 void* context);
