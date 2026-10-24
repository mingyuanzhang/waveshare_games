#pragma once

#include "EchoEngine.h"
#include "EchoLevels.h"

namespace echo_game {
class Raster;
constexpr int kFieldX = 30, kFieldY = 84, kFieldWidth = 300, kFieldHeight = 240;
constexpr size_t kRoomCount = kLevelCount;
constexpr size_t kRoomsPerPage = 3;
constexpr size_t kRoomPages = (kRoomCount + kRoomsPerPage - 1) / kRoomsPerPage;
enum class Screen : uint8_t { Help, Playing, Menu, Rooms, Result };
enum class Action : uint8_t { None, Changed, Called };
class EchoApp {
 public:
  void initialize(size_t room, const uint32_t* scores, bool first_visit);
  Action tap(int x, int y, uint64_t now_ms);
  Action press_step();
  bool set_battery(int millivolts, int percent);
  void draw(uint16_t* pixels, uint64_t now_ms, bool animation_only = false) const;
  bool animating(uint64_t now_ms) const;
  void stamp_ping(uint64_t now_ms) { ping_started_ = now_ms; }
  bool take_save_request();
  size_t room_index() const { return room_; }
  uint32_t best_score(size_t room) const { return scores_[room]; }
  const Run& run() const { return run_; }
  Screen screen() const { return screen_; }
  size_t echo_count() const { return echoes_.count; }

 private:
  struct Blip {
    float cosine, sine, start, fade, peak, length, thickness;
    uint32_t color;
  };
  void load_room(size_t room);
  void show_rooms(Screen return_to);
  void prepare_ping(uint64_t now_ms);
  void draw_field(Raster& canvas, uint64_t now_ms) const;
  Run run_;
  EchoBuffer echoes_;
  Blip blips_[kMaxEchoes] = {};
  size_t room_ = 0, rooms_page_ = 0;
  uint32_t scores_[kRoomCount] = {};
  Screen screen_ = Screen::Help, rooms_return_ = Screen::Menu;
  bool first_help_ = true, save_pending_ = false, ping_active_ = false;
  int battery_mv_ = -1, battery_percent_ = -1;
  uint64_t ping_started_ = 0;
  float ping_end_ = 0;
};
}  // namespace echo_game

