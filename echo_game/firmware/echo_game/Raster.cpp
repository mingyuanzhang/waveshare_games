#include "Raster.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace echo_game {
namespace {
struct Glyph { char letter; uint8_t rows[7]; };
const Glyph kGlyphs[] = {
  {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
  {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
  {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
  {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
  {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
  {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
  {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,25,21,19,19,17}},
  {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
  {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
  {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
  {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
  {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
  {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
  {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
  {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
  {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
  {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
  {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
  {':',{0,4,4,0,4,4,0}}, {'.',{0,0,0,0,0,12,12}},
  {'/',{1,2,2,4,8,8,16}}, {'-',{0,0,0,31,0,0,0}},
  {'?',{14,17,1,2,4,0,4}}, {'+',{0,4,4,31,4,4,0}},
  {'%',{25,26,2,4,8,11,19}},
};
const uint8_t* glyph(char letter) {
  if (letter >= 'a' && letter <= 'z') letter -= 32;
  for (const auto& value : kGlyphs) if (value.letter == letter) return value.rows;
  return nullptr;
}
float clamp(float value) { return std::max(0.0f, std::min(1.0f, value)); }
}

uint16_t Raster::rgb565(uint32_t color) {
  return ((color >> 8) & 0xF800) | ((color >> 5) & 0x07E0) | ((color >> 3) & 0x001F);
}
void Raster::clear(uint32_t color) {
  std::fill(pixels_, pixels_ + 360 * 360, rgb565(color));
  clip();
}
void Raster::clip(int x, int y, int width, int height) {
  left_ = std::max(0, x); top_ = std::max(0, y);
  right_ = std::min(360, x + width); bottom_ = std::min(360, y + height);
}
void Raster::pixel(int x, int y, uint32_t color, float opacity) {
  if (x < left_ || x >= right_ || y < top_ || y >= bottom_ || opacity <= 0) return;
  uint16_t& target = pixels_[y * 360 + x];
  const uint16_t foreground = rgb565(color);
  if (opacity >= 0.995f) { target = foreground; return; }
  const int alpha = static_cast<int>(clamp(opacity) * 256);
  const int inverse = 256 - alpha;
  const int r = (((foreground >> 11) & 31) * alpha + ((target >> 11) & 31) * inverse) >> 8;
  const int g = (((foreground >> 5) & 63) * alpha + ((target >> 5) & 63) * inverse) >> 8;
  const int b = ((foreground & 31) * alpha + (target & 31) * inverse) >> 8;
  target = static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
void Raster::rect(int x, int y, int width, int height, uint32_t color, float opacity) {
  if (opacity >= 0.995f) {
    const int x0 = std::max(left_, x), x1 = std::min(right_, x + width);
    if (x0 >= x1) return;
    const uint16_t packed = rgb565(color);
    for (int py = std::max(top_, y); py < std::min(bottom_, y + height); ++py)
      std::fill(pixels_ + py * 360 + x0, pixels_ + py * 360 + x1, packed);
    return;
  }
  for (int py = std::max(top_, y); py < std::min(bottom_, y + height); ++py)
    for (int px = std::max(left_, x); px < std::min(right_, x + width); ++px)
      pixel(px, py, color, opacity);
}
void Raster::line(float ax, float ay, float bx, float by, float width, uint32_t color, float opacity) {
  if (opacity <= 0.004f) return;
  const float dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
  const float inverse_length2 = length2 ? 1.0f / length2 : 0;
  const float radius = width / 2, reach = radius + 0.5f;
  const int x0 = std::max(left_, static_cast<int>(std::floor(std::min(ax, bx) - reach)));
  const int x1 = std::min(right_, static_cast<int>(std::ceil(std::max(ax, bx) + reach)) + 1);
  const int y0 = std::max(top_, static_cast<int>(std::floor(std::min(ay, by) - reach)));
  const int y1 = std::min(bottom_, static_cast<int>(std::ceil(std::max(ay, by) + reach)) + 1);
  // The capsule lies inside the infinite strip around its center line. Bound
  // each row by that strip, leaving a pixel of slack for floating-point error.
  const float slope = std::abs(dy) > 1 ? dx / dy : 0;
  const float span = std::abs(dy) > 1 ? reach * std::sqrt(length2) / std::abs(dy) + 1 : 0;
  const bool narrow = std::abs(dy) > 1 && span * 2 + 4 < x1 - x0;
  for (int y = y0; y < y1; ++y) {
    int begin = x0, end = x1;
    if (narrow) {
      const float center = ax + (y + 0.5f - ay) * slope;
      begin = std::max(begin, static_cast<int>(std::floor(center - span)) - 1);
      end = std::min(end, static_cast<int>(std::ceil(center + span)) + 1);
    }
    for (int x = begin; x < end; ++x) {
      const float t = clamp(((x + 0.5f - ax) * dx + (y + 0.5f - ay) * dy) * inverse_length2);
      const float ox = x + 0.5f - ax - dx * t, oy = y + 0.5f - ay - dy * t;
      const float distance2 = ox * ox + oy * oy;
      if (distance2 > reach * reach) continue;
      pixel(x, y, color, opacity * clamp(reach - std::sqrt(distance2)));
    }
  }
}
void Raster::circle(float cx, float cy, float radius, float width, uint32_t color, float opacity) {
  if (radius <= 0 || opacity <= 0.004f) return;
  const float reach = width / 2 + 0.5f;
  const float inner = std::max(0.0f, radius - reach), outer = radius + reach;
  const float inner2 = inner * inner, outer2 = outer * outer;
  const int x0 = std::max(left_, static_cast<int>(cx - outer));
  const int x1 = std::min(right_, static_cast<int>(cx + outer) + 1);
  for (int y = std::max(top_, static_cast<int>(cy - outer)); y < std::min(bottom_, static_cast<int>(cy + outer) + 1); ++y) {
    const float dy = y + 0.5f - cy, dy2 = dy * dy;
    const float outside = std::sqrt(std::max(0.0f, outer2 - dy2));
    const int begin = std::max(x0, static_cast<int>(std::floor(cx - 0.5f - outside)) - 1);
    const int end = std::min(x1, static_cast<int>(std::ceil(cx - 0.5f + outside)) + 2);
    const float inside = std::sqrt(std::max(0.0f, inner2 - dy2));
    const int left_end = std::min(end, static_cast<int>(std::floor(cx - 0.5f - inside)) + 2);
    const int right_begin = std::max(left_end, static_cast<int>(std::ceil(cx - 0.5f + inside)) - 1);
    // Visit the two thin arcs rather than scanning the empty disk between them.
    // Retain the original distance and coverage calculation at every candidate.
    for (int x = begin; x < end; ++x) {
      if (x >= left_end && x < right_begin) { x = right_begin - 1; continue; }
      const float dx = x + 0.5f - cx, distance2 = dx * dx + dy2;
      if (distance2 < inner2 || distance2 > outer2) continue;
      pixel(x, y, color, opacity * clamp(reach - std::abs(std::sqrt(distance2) - radius)));
    }
  }
}
void Raster::disc(float cx, float cy, float radius, uint32_t color, float opacity) {
  for (int y = std::max(top_, static_cast<int>(cy - radius - 1)); y < std::min(bottom_, static_cast<int>(cy + radius + 2)); ++y)
    for (int x = std::max(left_, static_cast<int>(cx - radius - 1)); x < std::min(right_, static_cast<int>(cx + radius + 2)); ++x) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy, distance2 = dx * dx + dy * dy;
      if (distance2 > (radius + 0.5f) * (radius + 0.5f)) continue;
      pixel(x, y, color, opacity * clamp(radius + 0.5f - std::sqrt(distance2)));
    }
}
void Raster::rounded_rect(int x, int y, int width, int height, int radius, uint32_t color) {
  rect(x + radius, y, width - radius * 2, height, color);
  rect(x, y + radius, width, height - radius * 2, color);
  disc(x + radius, y + radius, radius, color);
  disc(x + width - radius, y + radius, radius, color);
  disc(x + radius, y + height - radius, radius, color);
  disc(x + width - radius, y + height - radius, radius, color);
}
void Raster::text(const char* value, int cx, int cy, int scale, uint32_t color) {
  const int length = static_cast<int>(std::strlen(value));
  int x = cx - (length * 6 - 1) * scale / 2;
  const int y = cy - 7 * scale / 2;
  for (int i = 0; i < length; ++i, x += 6 * scale) {
    const uint8_t* rows = glyph(value[i]);
    if (!rows) continue;
    for (int row = 0; row < 7; ++row) for (int column = 0; column < 5; ++column)
      if (rows[row] & (1 << (4 - column))) rect(x + column * scale, y + row * scale, scale, scale, color);
  }
}
}  // namespace echo_game
