#include "EchoApp.h"
#include "EchoLevels.h"
#include "Raster.h"

#include <algorithm>
#include <cmath>
#include <stdio.h>

namespace echo_game {
namespace {
constexpr uint32_t kBackground = 0x04060A, kPanel = 0x0E1A26;
constexpr uint32_t kText = 0xEAF6FF, kMuted = 0x7D96A8, kBlue = 0x7FD8FF;
constexpr uint32_t kAmber = 0xFFBC74, kGreen = 0x69DFBC;
constexpr float kCenterX = 180, kCenterY = 180, kRim = 78;
const uint32_t kMaterialColor[] = {0x7FD8FF, 0xEAF6FF, 0x5C9BFF, 0xFF9C74};
bool inside(int x, int y, int ax, int ay, int width, int height) {
  return x >= ax && x < ax + width && y >= ay && y < ay + height;
}
float clamp(float x) { return std::max(0.0f, std::min(1.0f, x)); }
float ease(float x) { x = clamp(x); return x < 0.5f ? 2 * x * x : 1 - (-2 * x + 2) * (-2 * x + 2) / 2; }
void button(Raster& canvas, int x, int y, int width, int height, const char* label, bool enabled = true) {
  canvas.rounded_rect(x, y, width, height, 11, enabled ? kPanel : 0x08121B);
  canvas.text(label, x + width / 2, y + height / 2, 2, enabled ? kText : 0x3D5261);
}
const char* grade(uint32_t score) {
  if (score >= 950) return "FLAWLESS";
  if (score >= 820) return "SHARP";
  if (score >= 650) return "CAPABLE";
  if (score >= 450) return "CLUMSY";
  return "LOST";
}
}

void EchoApp::initialize(size_t room, const uint32_t* scores, bool first_visit) {
  for (size_t i = 0; i < kRoomCount; ++i) scores_[i] = scores ? std::min<uint32_t>(1000, scores[i]) : 0;
  load_room(room);
  first_help_ = first_visit;
  screen_ = first_visit ? Screen::Help : Screen::Playing;
  save_pending_ = false;
}
void EchoApp::load_room(size_t room) {
  room_ = room < kRoomCount ? room : 0;
  run_.load(kLevels[room_]);
  ping_active_ = false;
  echoes_.count = 0;
  screen_ = Screen::Playing;
  save_pending_ = true;
}
void EchoApp::show_rooms(Screen return_to) {
  rooms_return_ = return_to;
  rooms_page_ = room_ / kRoomsPerPage;
  screen_ = Screen::Rooms;
}
bool EchoApp::take_save_request() {
  const bool requested = save_pending_;
  save_pending_ = false;
  return requested;
}
bool EchoApp::set_battery(int millivolts, int percent) {
  const bool changed = millivolts != battery_mv_ || percent != battery_percent_;
  battery_mv_ = millivolts;
  battery_percent_ = percent;
  return changed && screen_ == Screen::Menu;
}
Action EchoApp::press_step() {
  if (screen_ != Screen::Playing) return Action::None;
  run_.step();
  ping_active_ = false;
  if (run_.status() == Status::Escaped) {
    screen_ = Screen::Result;
    if (run_.score() > scores_[room_]) { scores_[room_] = run_.score(); save_pending_ = true; }
  }
  return Action::Changed;
}
void EchoApp::prepare_ping(uint64_t now_ms) {
  emit(run_.level(), run_.position(), run_.heading(), echoes_);
  ping_started_ = now_ms;
  ping_active_ = true;
  ping_end_ = 1300;
  for (size_t i = 0; i < echoes_.count; ++i) {
    const Echo& echo = echoes_.values[i];
    const float arrival = static_cast<float>(echo.distance / kSoundSpeed * 1000);
    const float fade = echo.bounces == 1 ? 900 : 520;
    // These values are only pixel positions and brightness. Use the FPU's
    // single-precision math here; geometry and scoring keep double precision.
    const float dx = static_cast<float>(echo.point.x - run_.position().x);
    const float dy = static_cast<float>(echo.point.y - run_.position().y);
    const float length = std::sqrt(dx * dx + dy * dy);
    const float intensity = static_cast<float>(echo.intensity);
    blips_[i] = {
      length ? dx / length : 1.0f, length ? dy / length : 0.0f,
      std::max(0.0f, arrival - 240), fade,
      std::min(1.0f, std::pow(intensity, 0.6f) * 1.05f),
      kRim * (0.1f + intensity * 0.3f),
      std::max(1.5f, kRim * (0.012f + intensity * 0.03f)),
      kMaterialColor[static_cast<size_t>(echo.material)]
    };
    ping_end_ = std::max(ping_end_, blips_[i].start + 240 + fade);
  }
}
bool EchoApp::animating(uint64_t now_ms) const {
  return screen_ == Screen::Playing && ping_active_ &&
         now_ms >= ping_started_ && now_ms - ping_started_ <= ping_end_;
}

Action EchoApp::tap(int x, int y, uint64_t now_ms) {
  if (x < 0 || x >= 360 || y < 0 || y >= 360) return Action::None;
  if (screen_ == Screen::Help) {
    if (!inside(x, y, 108, 266, 144, 44)) return Action::None;
    screen_ = Screen::Playing;
    first_help_ = false;
    save_pending_ = true;
    return Action::Changed;
  }
  if (screen_ == Screen::Menu) {
    if (x < 72 || x >= 288) return Action::None;
    if (y >= 76 && y < 116) screen_ = Screen::Playing;
    else if (y >= 120 && y < 160) load_room(room_);
    else if (y >= 164 && y < 204) show_rooms(Screen::Menu);
    else if (y >= 208 && y < 248) { first_help_ = false; screen_ = Screen::Help; }
    else if (y >= 252 && y < 292) {
      run_.surrender(); screen_ = Screen::Result; ping_active_ = false;
    } else return Action::None;
    return Action::Changed;
  }
  if (screen_ == Screen::Rooms) {
    for (size_t row = 0; row < kRoomsPerPage; ++row) {
      const size_t index = rooms_page_ * kRoomsPerPage + row;
      if (index < kRoomCount && inside(x, y, 58, 82 + static_cast<int>(row) * 60, 244, 50)) {
        load_room(index);
        return Action::Changed;
      }
    }
    if (inside(x, y, 60, 272, 72, 40) && rooms_page_ > 0) --rooms_page_;
    else if (inside(x, y, 228, 272, 72, 40) && rooms_page_ + 1 < kRoomPages) ++rooms_page_;
    else if (inside(x, y, 140, 272, 80, 40)) screen_ = rooms_return_;
    else return Action::None;
    return Action::Changed;
  }
  if (screen_ == Screen::Result) {
    if (inside(x, y, 82, 280, 94, 42)) load_room(room_);
    else if (inside(x, y, 186, 280, 92, 42)) {
      if (room_ + 1 < kRoomCount) load_room(room_ + 1);
      else show_rooms(Screen::Result);
    } else return Action::None;
    return Action::Changed;
  }

  if (inside(x, y, 102, 14, 156, 32)) { screen_ = Screen::Menu; return Action::Changed; }
  if (!inside(x, y, kFieldX, kFieldY, kFieldWidth, kFieldHeight)) return Action::None;
  const double dx = x - kCenterX, dy = y - kCenterY;
  if (dx * dx + dy * dy < 18 * 18) return Action::None;
  const double heading = std::atan2(dy, dx);
  run_.call(heading);
  prepare_ping(now_ms);
  return Action::Called;
}

void EchoApp::draw(uint16_t* pixels, uint64_t now_ms, bool animation_only) const {
  Raster canvas(pixels);
  if (animation_only && screen_ == Screen::Playing) {
    canvas.rect(kFieldX, kFieldY, kFieldWidth, kFieldHeight, kBackground);
    draw_field(canvas, now_ms);
    return;
  }
  canvas.clear(kBackground);
  char value[64];
  if (screen_ == Screen::Help) {
    canvas.circle(180, 165, 134, 1, 0x12303F);
    canvas.text("ECHO", 180, 61, 4, kBlue);
    canvas.text("BLIND ESCAPE", 180, 100, 2, kMuted);
    canvas.text("TAP A DIRECTION", 180, 141, 2, kText);
    canvas.text("TO SEND A CALL", 180, 161, 2, kText);
    canvas.text("PRESS BOOT TO STEP", 180, 205, 2, kText);
    canvas.text("WALK WHERE YOU FACE", 180, 229, 1, kMuted);
    canvas.text("FIND THE SILENT OPENING", 180, 251, 1, kBlue);
    button(canvas, 108, 266, 144, 44, first_help_ ? "PLAY" : "BACK");
    return;
  }
  if (screen_ == Screen::Menu) {
    canvas.text("MENU", 180, 40, 3, kBlue);
    const char* names[] = {"CONTINUE", "RESTART", "ROOMS", "HOW TO PLAY", "REVEAL ROOM"};
    for (int i = 0; i < 5; ++i) button(canvas, 72, 76 + i * 44, 216, 40, names[i]);
    if (battery_mv_ < 0) snprintf(value, sizeof(value), "BATTERY UNKNOWN");
    else if (battery_percent_ >= 0) snprintf(value, sizeof(value), "BAT %d%%  %d.%02dV", battery_percent_, battery_mv_ / 1000, (battery_mv_ % 1000) / 10);
    else snprintf(value, sizeof(value), "BAT %d.%02dV", battery_mv_ / 1000, (battery_mv_ % 1000) / 10);
    canvas.text(value, 180, 315, 2, kMuted);
    return;
  }
  if (screen_ == Screen::Rooms) {
    canvas.text("ROOMS", 180, 42, 3, kBlue);
    snprintf(value, sizeof(value), "%u/%u", static_cast<unsigned>(rooms_page_ + 1), static_cast<unsigned>(kRoomPages));
    canvas.text(value, 180, 65, 1, kMuted);
    for (size_t row = 0; row < kRoomsPerPage; ++row) {
      const size_t i = rooms_page_ * kRoomsPerPage + row;
      if (i >= kRoomCount) break;
      const int y = 82 + static_cast<int>(row) * 60;
      canvas.rounded_rect(58, y, 244, 50, 11, i == room_ ? 0x152B3B : kPanel);
      snprintf(value, sizeof(value), "%02u %s", static_cast<unsigned>(i + 1), kLevels[i].name);
      canvas.text(value, 180, y + 17, 2, kText);
      if (scores_[i]) snprintf(value, sizeof(value), "BEST %lu", static_cast<unsigned long>(scores_[i]));
      else snprintf(value, sizeof(value), "UNPLAYED");
      canvas.text(value, 180, y + 39, 1, kMuted);
    }
    button(canvas, 60, 272, 72, 40, "PREV", rooms_page_ > 0);
    button(canvas, 140, 272, 80, 40, "BACK");
    button(canvas, 228, 272, 72, 40, "NEXT", rooms_page_ + 1 < kRoomPages);
    return;
  }
  if (screen_ == Screen::Result) {
    const bool escaped = run_.status() == Status::Escaped;
    canvas.text(escaped ? "ESCAPED" : "REVEALED", 180, 28, 3, escaped ? kGreen : kAmber);
    if (escaped) snprintf(value, sizeof(value), "%lu %s", static_cast<unsigned long>(run_.score()), grade(run_.score()));
    else snprintf(value, sizeof(value), "%s", run_.level().name);
    canvas.text(value, 180, 58, 2, kText);
    const Level& level = run_.level();
    const double scale = std::min(210 / level.width, 186 / level.height);
    const double ox = 180 - level.width * scale / 2, oy = 180 - level.height * scale / 2;
    for (size_t i = 0; i < level.wall_count; ++i) {
      const Segment& wall = level.walls[i];
      canvas.line(ox + wall.a.x * scale, oy + wall.a.y * scale,
                  ox + wall.b.x * scale, oy + wall.b.y * scale, 1.7f,
                  kMaterialColor[static_cast<size_t>(wall.material)], 0.8f);
    }
    canvas.line(ox + level.exit_a.x * scale, oy + level.exit_a.y * scale,
                ox + level.exit_b.x * scale, oy + level.exit_b.y * scale, 3, kGreen);
    for (size_t i = 1; i < run_.path_count(); ++i) {
      const Vec a = run_.path_at(i - 1), b = run_.path_at(i);
      canvas.line(ox + a.x * scale, oy + a.y * scale, ox + b.x * scale, oy + b.y * scale, 1.5f, 0x2B5CE6);
    }
    const Vec pos = run_.position();
    canvas.disc(ox + level.start.x * scale, oy + level.start.y * scale, 3, 0x2B5CE6);
    canvas.disc(ox + pos.x * scale, oy + pos.y * scale, 3, kText);
    button(canvas, 82, 280, 94, 42, "RETRY");
    button(canvas, 186, 280, 92, 42, room_ + 1 < kRoomCount ? "NEXT" : "ROOMS");
    return;
  }

  draw_field(canvas, now_ms);
  button(canvas, 102, 14, 156, 32, "ECHO MENU");
  snprintf(value, sizeof(value), "%u/%u %s", static_cast<unsigned>(room_ + 1), static_cast<unsigned>(kRoomCount), run_.level().name);
  canvas.text(value, 180, 60, 2, kText);
  snprintf(value, sizeof(value), "CALLS %lu   MOVES %lu", static_cast<unsigned long>(run_.calls()), static_cast<unsigned long>(run_.moves()));
  canvas.text(value, 180, 76, 1, kMuted);
  canvas.text("BOOT BUTTON: STEP", 180, 333, 1, kBlue);
}

void EchoApp::draw_field(Raster& canvas, uint64_t now_ms) const {
  canvas.clip(kFieldX, kFieldY, kFieldWidth, kFieldHeight);
  canvas.circle(kCenterX, kCenterY, kRim, 1, 0x12303F);
  canvas.line(kCenterX, kCenterY,
              kCenterX + std::cos(run_.heading()) * kRim * 0.72,
              kCenterY + std::sin(run_.heading()) * kRim * 0.72,
              2, run_.blocked() ? 0xC2453A : 0x24506B);
  if (ping_active_ && now_ms >= ping_started_) {
    const float age = static_cast<float>(now_ms - ping_started_);
    if (age < 1300) {
      const float alpha = age < 80 ? 0.42f * age / 80 : 0.42f * (1 - ease((age - 80) / 1220));
      canvas.circle(kCenterX, kCenterY, 186.44f * age / 1300, 1.5f, 0x4FB4E0, alpha);
    }
    for (size_t i = 0; i < echoes_.count; ++i) {
      const Blip& blip = blips_[i];
      const float dt = age - blip.start;
      if (dt < 0 || dt > 240 + blip.fade) continue;
      const float alpha = blip.peak * (dt < 240 ? ease(dt / 240) : 1 - ease((dt - 240) / blip.fade));
      const float t = clamp(dt / 240), travel = 1 - (1 - t) * (1 - t);
      const float radius = kRim * (1.34f - 0.34f * travel);
      const float x = kCenterX + blip.cosine * radius, y = kCenterY + blip.sine * radius;
      const float half = std::max(0.0f, (blip.length - blip.thickness) / 2);
      canvas.line(x - blip.cosine * half, y - blip.sine * half,
                  x + blip.cosine * half, y + blip.sine * half,
                  blip.thickness, blip.color, alpha);
    }
  }
  canvas.disc(kCenterX, kCenterY, 3, 0x2B5CE6);
  canvas.clip();
  const char* hint = run_.blocked() ? "BLOCKED"
                   : ping_active_ && echoes_.count == 0 ? "NO RETURN" : "TAP TO CALL";
  canvas.text(hint, 180, 307, 1, kMuted);
}
}  // namespace echo_game
