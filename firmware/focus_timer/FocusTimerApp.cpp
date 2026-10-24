#include "FocusTimerApp.h"

#include <Arduino.h>
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>

#include "Countdown.h"
#include "DisplaySync.h"
#include "Display_ST77916.h"
#include "I2C_Driver.h"
#include "LVGL_Driver.h"

namespace {
constexpr uint32_t kBackground = 0x10151C;
constexpr uint32_t kSurface = 0x242D39;
constexpr uint32_t kTeal = 0x69DFBC;
constexpr uint32_t kText = 0xF1F5F9;
constexpr uint32_t kMuted = 0x9BAABD;
constexpr uint32_t kAmber = 0xFFC971;

struct Preset {
  uint32_t seconds;
  const char* label;
};

#ifdef FOCUS_TIMER_QUICK_TEST
Preset presets[] = {{5, "5s"}, {25, "25s"}, {50, "50s"}};
#else
Preset presets[] = {{5 * 60, "5m"}, {25 * 60, "25m"}, {50 * 60, "50m"}};
#endif

Countdown countdown(presets[1].seconds);
size_t selected_preset = 1;
lv_obj_t* preset_buttons[3] = {};
lv_obj_t* ring = nullptr;
lv_obj_t* time_label = nullptr;
lv_obj_t* status_label = nullptr;
lv_obj_t* action_label = nullptr;
lv_obj_t* action_button = nullptr;
lv_obj_t* footer = nullptr;
char displayed_time[16] = {};
Countdown::State displayed_state = Countdown::State::Finished;

uint64_t now_us() { return static_cast<uint64_t>(esp_timer_get_time()); }

const char* state_name(Countdown::State state) {
  switch (state) {
    case Countdown::State::Ready: return "Ready";
    case Countdown::State::Running: return "Focusing";
    case Countdown::State::Paused: return "Paused";
    case Countdown::State::Finished: return "Time for a break";
  }
  return "Ready";
}

lv_obj_t* label_at(lv_obj_t* parent, const char* text, int center_x,
                   int center_y, const lv_font_t* font, uint32_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, center_x - 180, center_y - 180);
  return label;
}

lv_obj_t* button_at(lv_obj_t* parent, int x, int y, int width, int height,
                    const char* text, lv_obj_t** label_out = nullptr) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, 14, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(kSurface), 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x344353), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(kText), 0);
  lv_obj_center(label);
  if (label_out) *label_out = label;
  return button;
}

void refresh_ui() {
  const uint64_t now = now_us();
  countdown.update(now);
  const uint32_t seconds = countdown.remaining_seconds(now);
  char formatted[16];
  snprintf(formatted, sizeof(formatted), "%02lu:%02lu",
           static_cast<unsigned long>(seconds / 60),
           static_cast<unsigned long>(seconds % 60));
  if (strcmp(formatted, displayed_time) != 0) {
    lv_label_set_text(time_label, formatted);
    strcpy(displayed_time, formatted);
  }

  lv_arc_set_value(ring, static_cast<int16_t>(
      countdown.remaining_us(now) * 1000 / countdown.duration_us()));

  const Countdown::State state = countdown.state();
  if (state == displayed_state) return;
  displayed_state = state;
  const bool finished = state == Countdown::State::Finished;
  const bool locked = state == Countdown::State::Running ||
                      state == Countdown::State::Paused;
  const uint32_t accent = finished ? kAmber : kTeal;
  lv_obj_set_style_arc_color(ring, lv_color_hex(accent), LV_PART_INDICATOR);
  lv_obj_set_style_text_color(time_label, lv_color_hex(accent), 0);
  lv_label_set_text(status_label, state_name(state));
  lv_label_set_text(action_label, state == Countdown::State::Running ? "Pause"
                                : state == Countdown::State::Paused ? "Resume"
                                : finished ? "Again" : "Start");
  lv_label_set_text(footer, finished ? "Session complete" : "Offline timer");

  for (size_t i = 0; i < 3; ++i) {
    if (locked) lv_obj_add_state(preset_buttons[i], LV_STATE_DISABLED);
    else lv_obj_clear_state(preset_buttons[i], LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(preset_buttons[i],
        lv_color_hex(i == selected_preset ? 0x326251 : kSurface), 0);
  }
  Serial.printf("[focus] %s, remaining=%lu seconds\n", state_name(state),
                static_cast<unsigned long>(seconds));
}

void preset_clicked(lv_event_t* event) {
  auto* preset = static_cast<Preset*>(lv_event_get_user_data(event));
  if (!countdown.choose_seconds(preset->seconds)) return;
  selected_preset = static_cast<size_t>(preset - presets);
  // Refresh selection colors even if the timer was already in Ready state.
  displayed_state = Countdown::State::Finished;
  refresh_ui();
}

void action_clicked(lv_event_t*) {
  countdown.toggle(now_us());
  refresh_ui();
}

void reset_clicked(lv_event_t*) {
  countdown.reset();
  refresh_ui();
}

void refresh_timer(lv_timer_t*) { refresh_ui(); }

void create_ui() {
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

#ifdef FOCUS_TIMER_QUICK_TEST
  const char* title = "TIMER TEST";
#else
  const char* title = "FOCUS";
#endif
  label_at(screen, title, 180, 26, &lv_font_montserrat_14, kMuted);

  for (size_t i = 0; i < 3; ++i) {
    preset_buttons[i] = button_at(screen, 84 + static_cast<int>(i) * 66,
                                   48, 60, 34, presets[i].label);
    lv_obj_add_event_cb(preset_buttons[i], preset_clicked, LV_EVENT_CLICKED,
                       &presets[i]);
  }

  ring = lv_arc_create(screen);
  lv_obj_set_size(ring, 196, 196);
  lv_obj_set_pos(ring, 82, 82);
  lv_arc_set_rotation(ring, 270);
  lv_arc_set_bg_angles(ring, 0, 360);
  lv_arc_set_range(ring, 0, 1000);
  lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(ring, lv_color_hex(kSurface), LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, 8, LV_PART_INDICATOR);

  time_label = label_at(screen, "25:00", 180, 175,
                       &lv_font_montserrat_48, kTeal);
  status_label = label_at(screen, "Ready", 180, 219,
                         &lv_font_montserrat_16, kMuted);

  action_button = button_at(screen, 80, 279, 98, 46, "Start", &action_label);
  lv_obj_set_style_bg_color(action_button, lv_color_hex(0x326251), 0);
  lv_obj_add_event_cb(action_button, action_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* reset = button_at(screen, 188, 279, 92, 46, "Reset");
  lv_obj_add_event_cb(reset, reset_clicked, LV_EVENT_CLICKED, nullptr);
  footer = label_at(screen, "Offline timer", 180, 340,
                    &lv_font_montserrat_14, kMuted);

  refresh_ui();
  lv_timer_create(refresh_timer, 100, nullptr);
}
}  // namespace

void focus_timer_setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("[focus] Starting Waveshare 1.85B focus timer");
  focus_lcd_sync_init();
  I2C_Init();
  Backlight_Init();
  LCD_Init();
  Lvgl_Init();
  Set_Backlight(60);
  create_ui();
  Serial.println("[focus] Display and touch initialized; tap Start");
}

void focus_timer_loop() {
  Lvgl_Loop();
  delay(5);
}

