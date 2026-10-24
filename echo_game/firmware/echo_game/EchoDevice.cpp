#include "EchoDevice.h"
#include "EchoApp.h"
#include "ActionButton.h"
#include "Benchmark.h"
#include "Display_ST77916.h"
#include "DisplaySync.h"
#include "I2C_Driver.h"

#include <Preferences.h>
#include <cstring>
#include <new>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

namespace {
constexpr int kWidth = 360, kHeight = 360, kStripRows = kEchoStripRows;
constexpr uint64_t kFrameMs = 40, kTouchMs = 10;
constexpr uint8_t kStepPin = 0, kGaugeAddress = 0x55;
echo_game::EchoApp app;
echo_game::ActionButton action_button;
Preferences preferences;
uint16_t* framebuffer = nullptr;
uint16_t* strip = nullptr;
bool preferences_ready = false, dirty = true, was_animating = false;
bool finger_down = false;
int press_x = 0, press_y = 0;
uint64_t last_touch = 0, last_frame = 0;
uint64_t last_battery = 0;
uint32_t animation_frames = 0;
uint64_t animation_work_us = 0;
uint32_t persisted_scores[echo_game::kRoomCount] = {};
size_t persisted_room = 0;
bool persisted_started = false;

uint64_t now_ms() { return static_cast<uint64_t>(esp_timer_get_time()) / 1000; }

// BQ27220 standard read commands, little-endian words (TI SLUUBD4).
// No gauge configuration or power-control commands are issued.
bool gauge_word(uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(kGaugeAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kGaugeAddress, static_cast<uint8_t>(2)) != 2) return false;
  const uint8_t low = Wire.read(), high = Wire.read();
  value = low | (static_cast<uint16_t>(high) << 8);
  return true;
}

void poll_battery(uint64_t time, bool force = false) {
  if (!force && time - last_battery < 5000) return;
  last_battery = time;
  uint16_t voltage = 0, percent = 0, current = 0, flags = 0;
  const bool valid_voltage = gauge_word(0x08, voltage) && voltage <= 6000;
  const bool valid_percent = gauge_word(0x2C, percent) && percent <= 100;
  const bool valid_current = gauge_word(0x0C, current);
  const bool valid_flags = gauge_word(0x0A, flags);
  dirty |= app.set_battery(valid_voltage ? voltage : -1, valid_percent ? percent : -1);
  if (Serial) {
    Serial.printf("[echo] battery voltage=%d mV soc=%d%% current=%d mA flags=%s0x%04x\n",
                  valid_voltage ? static_cast<int>(voltage) : -1,
                  valid_percent ? static_cast<int>(percent) : -1,
                  valid_current ? static_cast<int16_t>(current) : -32768,
                  valid_flags ? "" : "unavailable/", flags);
  }
}

bool read_touch(bool& down, int& x, int& y) {
  Wire.beginTransmission(static_cast<uint8_t>(CST816_ADDR));
  Wire.write(static_cast<uint8_t>(CST816_REG_GestureID));
  if (Wire.endTransmission(true) != 0) return false;
  if (Wire.requestFrom(static_cast<uint8_t>(CST816_ADDR), static_cast<uint8_t>(6)) != 6)
    return false;
  uint8_t data[6];
  for (auto& byte : data) byte = Wire.read();
  down = (data[1] & 0x0F) != 0;
  x = ((data[2] & 0x0F) << 8) | data[3];
  y = ((data[4] & 0x0F) << 8) | data[5];
  return true;
}

void save_progress() {
  if (!app.take_save_request() || !preferences_ready) return;
  if (!persisted_started && preferences.putBool("started", true)) persisted_started = true;
  if (persisted_room != app.room_index() && preferences.putUChar("room", static_cast<uint8_t>(app.room_index())))
    persisted_room = app.room_index();
  for (size_t i = 0; i < echo_game::kRoomCount; ++i) {
    if (persisted_scores[i] == app.best_score(i)) continue;
    char key[12];
    snprintf(key, sizeof(key), "score%u", static_cast<unsigned>(i));
    if (preferences.putUInt(key, app.best_score(i))) persisted_scores[i] = app.best_score(i);
  }
}

void poll_touch(uint64_t time) {
  if (time - last_touch < kTouchMs) return;
  last_touch = time;
  bool down;
  int x, y;
  if (!read_touch(down, x, y)) return;
  if (down && !finger_down) {
    if (x >= kWidth || y >= kHeight) return;
    press_x = x; press_y = y;
    finger_down = true;
  } else if (!down && finger_down) {
    finger_down = false;
    const int64_t started = esp_timer_get_time();
    const auto action = app.tap(press_x, press_y, time);
    dirty |= action != echo_game::Action::None;
    if (action == echo_game::Action::Called) {
      // Ray calculation time must not advance the visible echo animation.
      app.stamp_ping(now_ms());
      animation_frames = 0;
      animation_work_us = 0;
      Serial.printf("[echo] room=%u call=%lu echoes=%u calculation=%lld ms\n",
                    static_cast<unsigned>(app.room_index() + 1),
                    static_cast<unsigned long>(app.run().calls()),
                    static_cast<unsigned>(app.echo_count()),
                    (esp_timer_get_time() - started) / 1000);
    }
    save_progress();
  }
}

echo_game::FrameTimings present(uint64_t time, bool animation_only = false) {
  animation_only &= app.screen() == echo_game::Screen::Playing;
  const uint64_t started = static_cast<uint64_t>(esp_timer_get_time());
  app.draw(framebuffer, time, animation_only);
  const uint64_t rendered = static_cast<uint64_t>(esp_timer_get_time());
  const int x = animation_only ? echo_game::kFieldX : 0;
  const int top = animation_only ? echo_game::kFieldY : 0;
  const int width = animation_only ? echo_game::kFieldWidth : kWidth;
  const int bottom = animation_only ? top + echo_game::kFieldHeight : kHeight;
  for (int y = top; y < bottom; y += kStripRows) {
    const int rows = (y + kStripRows <= bottom) ? kStripRows : bottom - y;
    for (int row = 0; row < rows; ++row)
      std::memcpy(strip + row * width, framebuffer + (y + row) * kWidth + x, width * sizeof(uint16_t));
    // The official driver swaps byte order in place; keep that off the framebuffer.
    LCD_addWindow(x, y, x + width - 1, y + rows - 1, strip);
  }
  echo_game::FrameTimings timings;
  timings.render_us = rendered - started;
  timings.transfer_us = static_cast<uint64_t>(esp_timer_get_time()) - rendered;
  return timings;
}

uint64_t benchmark_clock(void*) { return static_cast<uint64_t>(esp_timer_get_time()); }
echo_game::FrameTimings benchmark_frame(uint64_t age, bool animation_only, void*) { return present(age, animation_only); }
void benchmark_report(const echo_game::BenchmarkResult& r, void*) {
  Serial.printf("[bench] ROOM,%u,%s,%u,%lu,%llu,%llu,%lu,%llu,%llu,%lu,%lu,%llu,%llu,%llu,%llu\n",
      static_cast<unsigned>(r.room + 1), echo_game::kLevels[r.room].name, static_cast<unsigned>(r.walls),
      static_cast<unsigned long>(r.calls), r.call_total_us, r.call_max_us,
      static_cast<unsigned long>(r.frames), r.frame_total_us, r.frame_max_us,
      static_cast<unsigned long>(r.frames_over_40ms), static_cast<unsigned long>(r.max_echoes),
      r.render_total_us, r.render_max_us, r.transfer_total_us, r.transfer_max_us);
  delay(1);
}
void benchmark_device() {
  // Keep the active app in internal RAM for representative measurements; store
  // only its snapshot in PSRAM. No benchmark action is written to Preferences.
  void* storage = heap_caps_malloc(sizeof(app), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!storage) { Serial.println("[bench] ERROR,no snapshot memory"); return; }
  auto* snapshot = new (storage) echo_game::EchoApp(app);
  Serial.printf("[bench] BEGIN,esp32s3,%u,25fps\n", static_cast<unsigned>(echo_game::kRoomCount));
  Serial.printf("[bench] CONFIG,cpu_mhz=%lu,strip_rows=%d\n", static_cast<unsigned long>(getCpuFrequencyMhz()), kStripRows);
  const bool okay = echo_game::run_benchmark(app, benchmark_clock, benchmark_frame, benchmark_report, nullptr);
  app = *snapshot;
  snapshot->~EchoApp();
  heap_caps_free(storage);
  while (Serial.available()) Serial.read();
  finger_down = false;
  action_button.initialize(digitalRead(kStepPin) == LOW, now_ms());
  present(now_ms());
  last_frame = now_ms();
  dirty = false;
  was_animating = app.animating(now_ms());
  Serial.println(okay ? "[bench] END" : "[bench] ERROR,call workload failed");
}
}

void echo_device_setup() {
  // PWR is handled by the board's power circuit. BOOT/GPIO0 is an active-low
  // application input after boot; never drive it as an output.
  pinMode(kStepPin, INPUT_PULLUP);
  action_button.initialize(digitalRead(kStepPin) == LOW, now_ms());
  Serial.begin(115200);
  Serial.setTxTimeoutMs(10);
  Serial.printf("[echo] Starting Echo: %u rooms\n", static_cast<unsigned>(echo_game::kRoomCount));
  framebuffer = static_cast<uint16_t*>(heap_caps_malloc(
      kWidth * kHeight * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  strip = static_cast<uint16_t*>(heap_caps_malloc(
      kWidth * kStripRows * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  ESP_ERROR_CHECK(framebuffer && strip ? ESP_OK : ESP_ERR_NO_MEM);

  preferences_ready = preferences.begin("echo-game", false);
  uint32_t scores[echo_game::kRoomCount] = {};
  size_t room = 0;
  bool first_visit = true;
  if (preferences_ready) {
    room = preferences.getUChar("room", 0);
    first_visit = !preferences.getBool("started", false);
    for (size_t i = 0; i < echo_game::kRoomCount; ++i) {
      char key[12];
      snprintf(key, sizeof(key), "score%u", static_cast<unsigned>(i));
      scores[i] = preferences.getUInt(key, 0);
    }
  }
  persisted_room = room;
  persisted_started = !first_visit;
  std::memcpy(persisted_scores, scores, sizeof(scores));
  app.initialize(room, scores, first_visit);
  echo_lcd_sync_init();
  I2C_Init();
  Wire.setClock(400000);
  Wire.setTimeOut(20);
  poll_battery(now_ms(), true);
  Backlight_Init();
  Set_Backlight(0);
  LCD_Init();
  present(now_ms());
  Set_Backlight(60);
  last_frame = now_ms();
  dirty = false;
  Serial.printf("[echo] Ready. Free internal=%u, PSRAM=%u. Touch calls; BOOT steps.\n",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}

void echo_device_loop() {
  while (Serial.available()) {
    if (Serial.read() == 'B') { benchmark_device(); break; }
  }
  if (action_button.update(digitalRead(kStepPin) == LOW, now_ms())) {
    dirty |= app.press_step() != echo_game::Action::None;
    save_progress();
  }
  poll_touch(now_ms());
  const uint64_t time = now_ms();
  poll_battery(time);
  const bool animating = app.animating(time);
  if (was_animating && !animating) {
    dirty = true;  // Clear the last fading echo, including after leaving the game.
    if (animation_frames) {
      Serial.printf("[echo] animation frames=%lu average draw+transfer=%llu ms\n",
                    static_cast<unsigned long>(animation_frames),
                    animation_work_us / animation_frames / 1000);
    }
  }
  if (dirty || (animating && time - last_frame >= kFrameMs)) {
    const int64_t started = esp_timer_get_time();
    present(time, !dirty && animating);
    last_frame = time;
    dirty = false;
    if (animating) {
      ++animation_frames;
      animation_work_us += esp_timer_get_time() - started;
    }
  }
  was_animating = animating;
  delay(1);
}
