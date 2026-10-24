#include "EchoEngine.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

namespace echo_game {
namespace {
constexpr double kEpsilon = 1e-4;
constexpr double kReflectivity[] = {0.8, 0.97, 0.5, 0.3};
struct RayWeight { double offset, energy; };
RayWeight ray_weights[kRays];
bool weights_ready = false;
// All shipped rooms fit in this cache. Larger scenes keep the exact ray path
// without filtering, so the cache does not impose a geometry limit.
constexpr size_t kWallCacheCapacity = 128;
struct WallBounds { float ax, ay, bx, by, error; };
WallBounds wall_bounds[kWallCacheCapacity];
const Segment* cached_walls = nullptr;
size_t cached_wall_count = 0;
bool prepare_walls(const Level& level) {
  if (level.wall_count > kWallCacheCapacity) return false;
  if (cached_walls == level.walls && cached_wall_count == level.wall_count) return true;
  for (size_t i = 0; i < level.wall_count; ++i) {
    const Segment& wall = level.walls[i];
    WallBounds& bounds = wall_bounds[i];
    bounds = {static_cast<float>(wall.a.x), static_cast<float>(wall.a.y),
              static_cast<float>(wall.b.x), static_cast<float>(wall.b.y), 0};
    bounds.error = (std::abs(bounds.ax) + std::abs(bounds.ay) +
                    std::abs(bounds.bx) + std::abs(bounds.by) + 1) * (16 * FLT_EPSILON);
  }
  cached_walls = level.walls;
  cached_wall_count = level.wall_count;
  return true;
}
void prepare_weights() {
  if (weights_ready) return;
  constexpr double spread = kPi / 4, half = spread / 2;
  for (size_t ray = 0; ray < kRays; ++ray) {
    const double offset = -half + spread * (ray + 0.5) / kRays;
    ray_weights[ray] = {offset, std::pow(std::max(0.0, std::cos(offset / half * kPi / 2)), 2.2)};
  }
  weights_ready = true;
}
}

bool cast_ray(const Level& level, Vec origin, Vec direction, Hit& hit) {
  const bool filtered = prepare_walls(level);
  const float px = static_cast<float>(origin.x), py = static_cast<float>(origin.y);
  const float dx = static_cast<float>(direction.x), dy = static_cast<float>(direction.y);
  const float direction_size = std::abs(dx) + std::abs(dy);
  const float origin_error = (std::abs(px) + std::abs(py) + 1) * (16 * FLT_EPSILON);
  const Segment* nearest = nullptr;
  double best = 0;
  for (size_t i = 0; i < level.wall_count; ++i) {
    if (filtered) {
      const WallBounds& bounds = wall_bounds[i];
      const float a = (bounds.ax - px) * dy - (bounds.ay - py) * dx;
      const float b = (bounds.bx - px) * dy - (bounds.by - py) * dx;
      const float margin = (bounds.error + origin_error) * direction_size;
      // Endpoints clearly on the same side cannot cross the ray's line.
      // The generous error bound keeps endpoint and near-parallel cases on
      // the original double-precision path; floats only reject obvious misses.
      if ((a > margin && b > margin) || (a < -margin && b < -margin)) continue;
    }
    const Segment& wall = level.walls[i];
    const double sx = wall.b.x - wall.a.x, sy = wall.b.y - wall.a.y;
    const double denominator = direction.x * sy - direction.y * sx;
    if (std::abs(denominator) < 1e-9) continue;
    const double ox = wall.a.x - origin.x, oy = wall.a.y - origin.y;
    const double sign = denominator < 0 ? -1 : 1;
    const double divisor = std::abs(denominator);
    const double across = (ox * direction.y - oy * direction.x) * sign;
    // Reject clearly invalid intersections before software double division.
    // Near segment endpoints, retain the original division and comparisons.
    const double margin = divisor * (8 * DBL_EPSILON);
    if (across < -margin || across > divisor + margin) continue;
    if (across <= margin || across >= divisor - margin) {
      const double u = across / divisor;
      if (u < 0 || u > 1) continue;
    }
    const double along = (ox * sy - oy * sx) * sign;
    if (along <= 0) continue;
    if (nearest) {
      const double limit = best * divisor;
      const double tolerance = (std::abs(limit) + std::abs(along)) * (8 * DBL_EPSILON);
      if (along > limit + tolerance) continue;
    }
    const double t = along / divisor;
    if (t <= kEpsilon || (nearest && t >= best)) continue;
    nearest = &wall;
    best = t;
  }
  if (!nearest) return false;
  const double sx = nearest->b.x - nearest->a.x, sy = nearest->b.y - nearest->a.y;
  const double length = std::hypot(sx, sy);
  Vec normal = {-sy / (length ? length : 1), sx / (length ? length : 1)};
  if (normal.x * direction.x + normal.y * direction.y > 0) {
    normal.x = -normal.x;
    normal.y = -normal.y;
  }
  hit = {best, {origin.x + direction.x * best, origin.y + direction.y * best},
         normal, nearest->material};
  return true;
}

bool segments_cross(Vec p1, Vec p2, Vec q1, Vec q2) {
  const double rx = p2.x - p1.x, ry = p2.y - p1.y;
  const double sx = q2.x - q1.x, sy = q2.y - q1.y;
  const double denominator = rx * sy - ry * sx;
  if (std::abs(denominator) < 1e-9) return false;
  const double dx = q1.x - p1.x, dy = q1.y - p1.y;
  const double t = (dx * sy - dy * sx) / denominator;
  const double u = (dx * ry - dy * rx) / denominator;
  return t >= 0 && t <= 1 && u >= 0 && u <= 1;
}

void emit(const Level& level, Vec origin, double heading, EchoBuffer& output) {
  prepare_weights();
  output.count = 0;
  constexpr double bounce_floor[] = {0, 1, 2.4, 5};
  for (size_t ray = 0; ray < kRays; ++ray) {
    const double angle = heading + ray_weights[ray].offset;
    double energy = ray_weights[ray].energy;
    Vec direction = {std::cos(angle), std::sin(angle)}, pos = origin;
    double travelled = 0;
    if (energy < 0.02) continue;
    for (uint8_t bounce = 1; bounce <= 2; ++bounce) {
      Hit hit;
      if (!cast_ray(level, pos, direction, hit)) break;
      travelled += hit.distance;
      const double round_trip = travelled * 2;
      const double incidence = std::abs(direction.x * hit.normal.x + direction.y * hit.normal.y);
      const double reflectivity = kReflectivity[static_cast<size_t>(hit.material)];
      const double spreading = std::pow(14.0 / std::max(14.0, round_trip), 2);
      const double intensity = energy * reflectivity * std::pow(incidence, 2.2) *
                               spreading * std::exp(-round_trip / 170);
      if (intensity >= 0.02 * bounce_floor[bounce]) {
        output.values[output.count++] = {
            hit.point, round_trip, std::min(1.0, intensity),
            std::atan2(hit.point.y - origin.y, hit.point.x - origin.x), hit.material, bounce};
      }
      energy *= reflectivity;
      if (energy < 0.02) break;
      const double dot = direction.x * hit.normal.x + direction.y * hit.normal.y;
      direction = {direction.x - 2 * dot * hit.normal.x, direction.y - 2 * dot * hit.normal.y};
      pos = {hit.point.x + direction.x * kEpsilon * 10,
             hit.point.y + direction.y * kEpsilon * 10};
    }
  }
  if (output.count > kMaxEchoes) {
    // Sort small indices, then permute the echoes once. Original ray order
    // breaks ties, preserving the browser's stable descending intensity sort.
    uint16_t order[kRawEchoes];
    for (size_t i = 0; i < output.count; ++i) order[i] = static_cast<uint16_t>(i);
    std::sort(order, order + output.count, [&output](uint16_t a, uint16_t b) {
      const double ia = output.values[a].intensity, ib = output.values[b].intensity;
      return ia == ib ? a < b : ia > ib;
    });
    for (size_t i = 0; i < output.count; ++i) {
      if (order[i] == i) continue;
      const Echo held = output.values[i];
      size_t destination = i;
      while (order[destination] != i) {
        const size_t source = order[destination];
        output.values[destination] = output.values[source];
        order[destination] = static_cast<uint16_t>(destination);
        destination = source;
      }
      output.values[destination] = held;
      order[destination] = static_cast<uint16_t>(destination);
    }
    output.count = kMaxEchoes;
  }
}

void Run::load(const Level& level) { prepare_weights(); prepare_walls(level); level_ = &level; restart(); }
void Run::restart() {
  position_ = level_->start;
  heading_ = level_->start_heading;
  calls_ = moves_ = 0;
  blocked_ = false;
  status_ = Status::Playing;
  path_start_ = path_count_ = 0;
  append_position();
}
void Run::call(double heading) {
  if (status_ != Status::Playing) return;
  heading_ = heading;
  ++calls_;
  blocked_ = false;
}
void Run::turn(double heading) {
  if (status_ != Status::Playing) return;
  heading_ = heading;
  blocked_ = false;
}
void Run::step() {
  if (status_ != Status::Playing) return;
  const Vec direction = {std::cos(heading_), std::sin(heading_)};
  Hit hit;
  const double limit = cast_ray(*level_, position_, direction, hit)
                           ? std::max(0.0, hit.distance - 0.35) : 3;
  const double travelled = std::min(3.0, limit);
  const Vec next = {position_.x + direction.x * travelled, position_.y + direction.y * travelled};
  if (segments_cross(position_, next, level_->exit_a, level_->exit_b)) status_ = Status::Escaped;
  position_ = next;
  blocked_ = travelled < 3 - 1e-6;
  ++moves_;
  append_position();
}
void Run::surrender() { if (status_ == Status::Playing) status_ = Status::Surrendered; }
uint32_t Run::score() const {
  if (status_ != Status::Escaped) return 0;
  const uint32_t calls = calls_ > level_->best_calls ? calls_ - level_->best_calls : 0;
  const uint32_t moves = moves_ > level_->best_moves ? moves_ - level_->best_moves : 0;
  const uint64_t cost = uint64_t(calls) * 30 + uint64_t(moves) * 35;
  return cost >= 940 ? 60 : 1000 - static_cast<uint32_t>(cost);
}
void Run::append_position() {
  if (path_count_ < kPathCapacity) {
    path_[(path_start_ + path_count_) % kPathCapacity] = position_;
    ++path_count_;
  } else {
    path_[path_start_] = position_;
    path_start_ = (path_start_ + 1) % kPathCapacity;
  }
}
Vec Run::path_at(size_t index) const { return path_[(path_start_ + index) % kPathCapacity]; }
}  // namespace echo_game

