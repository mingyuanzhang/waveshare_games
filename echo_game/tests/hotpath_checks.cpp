#include "EchoEngine.h"
#include "EchoLevels.h"
#include "Raster.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace echo_game;
namespace {
uint32_t random_state = 0x13579BDF;
double random_between(double low, double high) {
  random_state = random_state * 1664525u + 1013904223u;
  return low + (high - low) * (random_state / 4294967296.0);
}
float clamp(float value) { return std::max(0.0f, std::min(1.0f, value)); }

// Deliberately unfiltered intersection reference. It handles every wall with
// the original double divisions, including ties and exact segment endpoints.
bool reference_ray(const Level& level, Vec origin, Vec direction, Hit& hit) {
  const Segment* nearest = nullptr;
  double best = 0;
  for (size_t i = 0; i < level.wall_count; ++i) {
    const Segment& wall = level.walls[i];
    const double sx = wall.b.x - wall.a.x, sy = wall.b.y - wall.a.y;
    const double denominator = direction.x * sy - direction.y * sx;
    if (std::abs(denominator) < 1e-9) continue;
    const double ox = wall.a.x - origin.x, oy = wall.a.y - origin.y;
    const double t = (ox * sy - oy * sx) / denominator;
    const double u = (ox * direction.y - oy * direction.x) / denominator;
    if (t <= 1e-4 || u < 0 || u > 1 || (nearest && t >= best)) continue;
    nearest = &wall;
    best = t;
  }
  if (!nearest) return false;
  const double sx = nearest->b.x - nearest->a.x, sy = nearest->b.y - nearest->a.y;
  const double length = std::hypot(sx, sy);
  Vec normal = {-sy / (length ? length : 1), sx / (length ? length : 1)};
  if (normal.x * direction.x + normal.y * direction.y > 0) {
    normal.x = -normal.x; normal.y = -normal.y;
  }
  hit = {best, {origin.x + direction.x * best, origin.y + direction.y * best}, normal, nearest->material};
  return true;
}
size_t ray_cases = 0;
void check_ray(const Level& level, Vec origin, Vec direction) {
  Hit actual, expected;
  const bool found = reference_ray(level, origin, direction, expected);
  assert(cast_ray(level, origin, direction, actual) == found);
  if (found) {
    assert(std::abs(actual.distance - expected.distance) <= 1e-10 * std::max(1.0, expected.distance));
    assert(actual.point.x == expected.point.x && actual.point.y == expected.point.y);
    assert(actual.normal.x == expected.normal.x && actual.normal.y == expected.normal.y);
    assert(actual.material == expected.material);
  }
  ++ray_cases;
}

void check_rays() {
  for (size_t room = 0; room < kLevelCount; ++room) {
    const Level& level = kLevels[room];
    for (unsigned sample = 0; sample < 1200; ++sample) {
      const Vec origin = {random_between(-1, level.width + 1), random_between(-1, level.height + 1)};
      const double angle = random_between(-kPi, kPi);
      check_ray(level, origin, {std::cos(angle), std::sin(angle)});
    }
    for (size_t i = 0; i < level.wall_count; ++i) {
      const Segment& wall = level.walls[i];
      for (const Vec endpoint : {wall.a, wall.b}) {
        const double angle = std::atan2(endpoint.y - level.start.y, endpoint.x - level.start.x);
        for (double offset : {-1e-7, 0.0, 1e-7})
          check_ray(level, level.start, {std::cos(angle + offset), std::sin(angle + offset)});
      }
      const double parallel = std::atan2(wall.b.y - wall.a.y, wall.b.x - wall.a.x);
      for (double offset : {-1e-8, 0.0, 1e-8}) {
        const Vec direction = {std::cos(parallel + offset), std::sin(parallel + offset)};
        check_ray(level, {wall.a.x - 1e-7, wall.a.y + 1e-7}, direction);
        check_ray(level, {wall.a.x + 1e-7, wall.a.y - 1e-7}, direction);
      }
    }
  }
  // Exercise the uncached fallback with more walls than any shipped room.
  std::vector<Segment> walls(160, {{10, -10}, {10, 10}, Material::Metal});
  Level large = kLevels[0];
  large.walls = walls.data(); large.wall_count = walls.size();
  for (int i = 0; i < 100; ++i) {
    const double angle = random_between(-kPi, kPi);
    check_ray(large, {0, 0}, {std::cos(angle), std::sin(angle)});
  }
}

void check_raster() {
  constexpr size_t pixels = 360 * 360;
  std::vector<uint16_t> actual(pixels + 2), expected(pixels);
  for (unsigned sample = 0; sample < 400; ++sample) {
    std::fill(actual.begin(), actual.end(), 0x31AC);
    std::fill(expected.begin(), expected.end(), 0x31AC);
    Raster fast(actual.data() + 1), slow(expected.data());
    const int clip_x = static_cast<int>(random_between(0, 100));
    const int clip_y = static_cast<int>(random_between(0, 100));
    fast.clip(clip_x, clip_y, 280, 260); slow.clip(clip_x, clip_y, 280, 260);
    const float ax = random_between(-30, 390), ay = random_between(-30, 390);
    const float width = random_between(0.1, 8), opacity = random_between(0.002, 1);
    const float reach = width / 2 + 0.5f;
    if (sample % 2) {
      const float radius = random_between(0.1, 240);
      fast.circle(ax, ay, radius, width, 0x7FD8FF, opacity);
      const float inner = std::max(0.0f, radius - reach), outer = radius + reach;
      // Full viewport reference: no row spans or annulus bounds to share a
      // missed-pixel bug with the optimized renderer.
      for (int y = 0; y < 360; ++y) for (int x = 0; x < 360; ++x) {
        const float dx = x + 0.5f - ax, dy = y + 0.5f - ay, distance2 = dx * dx + dy * dy;
        if (opacity <= 0.004f || distance2 < inner * inner || distance2 > outer * outer) continue;
        slow.pixel(x, y, 0x7FD8FF, opacity * clamp(reach - std::abs(std::sqrt(distance2) - radius)));
      }
    } else {
      const float bx = sample % 10 == 0 ? ax : random_between(-30, 390);
      const float by = sample % 14 == 0 ? ay : random_between(-30, 390);
      fast.line(ax, ay, bx, by, width, 0x7FD8FF, opacity);
      const float dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
      const float reciprocal = length2 ? 1.0f / length2 : 0;
      for (int y = 0; y < 360; ++y) for (int x = 0; x < 360; ++x) {
        const float t = clamp(((x + 0.5f - ax) * dx + (y + 0.5f - ay) * dy) * reciprocal);
        const float ox = x + 0.5f - ax - dx * t, oy = y + 0.5f - ay - dy * t;
        const float distance2 = ox * ox + oy * oy;
        if (opacity <= 0.004f || distance2 > reach * reach) continue;
        slow.pixel(x, y, 0x7FD8FF, opacity * clamp(reach - std::sqrt(distance2)));
      }
    }
    assert(actual.front() == 0x31AC && actual.back() == 0x31AC);
    assert(std::equal(expected.begin(), expected.end(), actual.begin() + 1));
  }
}
}

void hotpath_checks() {
  check_rays();
  check_raster();
  std::printf("Passed %zu boundary/random ray comparisons and 400 full-viewport raster comparisons.\n", ray_cases);
}
