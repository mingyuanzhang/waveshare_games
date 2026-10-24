/* SPDX-License-Identifier: Apache-2.0
 * Minimal API declarations for the pinned Waveshare/Espressif implementation.
 */
#pragma once
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"
typedef void* es8311_handle_t;
enum es8311_resolution_t {ES8311_RESOLUTION_16=16,ES8311_RESOLUTION_18=18,ES8311_RESOLUTION_20=20,ES8311_RESOLUTION_24=24,ES8311_RESOLUTION_32=32};
typedef uint8_t es8311_mic_gain_t;
typedef uint8_t es8311_fade_t;
struct es8311_clock_config_t {bool mclk_inverted,sclk_inverted,mclk_from_mclk_pin;int mclk_frequency,sample_frequency;};
es8311_handle_t es8311_create(i2c_port_t port,uint16_t address);
void es8311_delete(es8311_handle_t dev);
esp_err_t es8311_init(es8311_handle_t dev,const es8311_clock_config_t* clock,es8311_resolution_t in,es8311_resolution_t out);
esp_err_t es8311_voice_volume_set(es8311_handle_t dev,int volume,int* actual);
esp_err_t es8311_microphone_config(es8311_handle_t dev,bool digital);
