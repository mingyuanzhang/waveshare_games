#pragma once

#include <stddef.h>
#include <stdint.h>

namespace echo_game {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSoundSpeed = 42.0;
constexpr size_t kRays = 200;
constexpr size_t kRawEchoes = kRays * 2;
constexpr size_t kMaxEchoes = 300;
constexpr size_t kPathCapacity = 512;

struct Vec { double x, y; };
enum class Material : uint8_t { Stone, Metal, Water, Soft };
struct Segment { Vec a, b; Material material; };
struct Level {
  const char* id;
  const char* name;
  double width, height;
  const Segment* walls;
  size_t wall_count;
  Vec start;
  double start_heading;
  Vec exit_a, exit_b;
  uint32_t best_calls, best_moves;
};
struct Hit { double distance; Vec point, normal; Material material; };
struct Echo {
  Vec point;
  double distance, intensity, bearing;
  Material material;
  uint8_t bounces;
};
struct EchoBuffer { Echo values[kRawEchoes]; size_t count = 0; };

bool cast_ray(const Level& level, Vec origin, Vec direction, Hit& hit);
bool segments_cross(Vec p1, Vec p2, Vec q1, Vec q2);
void emit(const Level& level, Vec origin, double heading, EchoBuffer& output);

enum class Status : uint8_t { Playing, Escaped, Surrendered };
class Run {
 public:
  void load(const Level& level);
  void call(double heading);
  void turn(double heading);
  void step();
  void surrender();
  void restart();
  uint32_t score() const;
  const Level& level() const { return *level_; }
  Vec position() const { return position_; }
  double heading() const { return heading_; }
  uint32_t calls() const { return calls_; }
  uint32_t moves() const { return moves_; }
  bool blocked() const { return blocked_; }
  Status status() const { return status_; }
  size_t path_count() const { return path_count_; }
  Vec path_at(size_t index) const;

 private:
  void append_position();
  const Level* level_ = nullptr;
  Vec position_ = {0, 0};
  double heading_ = 0;
  uint32_t calls_ = 0, moves_ = 0;
  bool blocked_ = false;
  Status status_ = Status::Playing;
  Vec path_[kPathCapacity] = {};
  size_t path_start_ = 0, path_count_ = 0;
};
}  // namespace echo_game

