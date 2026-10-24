#pragma once

#include <stdint.h>

namespace echo_game {
class Raster {
 public:
  explicit Raster(uint16_t* pixels) : pixels_(pixels) {}
  void clear(uint32_t color);
  void clip(int x = 0, int y = 0, int width = 360, int height = 360);
  void pixel(int x, int y, uint32_t color, float opacity = 1);
  void rect(int x, int y, int width, int height, uint32_t color, float opacity = 1);
  void line(float ax, float ay, float bx, float by, float width, uint32_t color, float opacity = 1);
  void circle(float x, float y, float radius, float width, uint32_t color, float opacity = 1);
  void disc(float x, float y, float radius, uint32_t color, float opacity = 1);
  void rounded_rect(int x, int y, int width, int height, int radius, uint32_t color);
  void text(const char* value, int center_x, int center_y, int scale, uint32_t color);
  static uint16_t rgb565(uint32_t color);

 private:
  uint16_t* pixels_;
  int left_ = 0, top_ = 0, right_ = 360, bottom_ = 360;
};
}  // namespace echo_game

