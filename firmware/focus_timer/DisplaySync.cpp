#include "DisplaySync.h"

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
SemaphoreHandle_t color_done = nullptr;
}

void focus_lcd_sync_init() {
  color_done = xSemaphoreCreateBinary();
  ESP_ERROR_CHECK(color_done ? ESP_OK : ESP_ERR_NO_MEM);
}

bool IRAM_ATTR focus_lcd_color_done(esp_lcd_panel_io_handle_t,
                                  esp_lcd_panel_io_event_data_t*, void*) {
  BaseType_t wake = pdFALSE;
  xSemaphoreGiveFromISR(color_done, &wake);
  return wake == pdTRUE;
}

void focus_lcd_wait() {
  // LVGL may reuse its draw buffer only after the SPI DMA transfer finishes.
  ESP_ERROR_CHECK(xSemaphoreTake(color_done, pdMS_TO_TICKS(1000)) == pdTRUE
                      ? ESP_OK
                      : ESP_ERR_TIMEOUT);
}

