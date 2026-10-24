#include "DisplaySync.h"

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
SemaphoreHandle_t color_done = nullptr;
}

void echo_lcd_sync_init() {
  color_done = xSemaphoreCreateBinary();
  ESP_ERROR_CHECK(color_done ? ESP_OK : ESP_ERR_NO_MEM);
}

bool IRAM_ATTR echo_lcd_color_done(esp_lcd_panel_io_handle_t,
                                 esp_lcd_panel_io_event_data_t*, void*) {
  BaseType_t wake = pdFALSE;
  xSemaphoreGiveFromISR(color_done, &wake);
  return wake == pdTRUE;
}

void echo_lcd_wait() {
  // The DMA strip may be overwritten only after its transfer finishes.
  ESP_ERROR_CHECK(xSemaphoreTake(color_done, pdMS_TO_TICKS(1000)) == pdTRUE
                      ? ESP_OK : ESP_ERR_TIMEOUT);
}
