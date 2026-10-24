#include "Benchmark.h"

#include <algorithm>
#include <cmath>

namespace echo_game {
bool run_benchmark(EchoApp& app, BenchmarkClock clock, BenchmarkFrame frame,
                   BenchmarkReport report, void* context) {
  const uint64_t ages[] = {80, 300, 700, 1200};
  for (size_t room = 0; room < kRoomCount; ++room) {
    BenchmarkResult result;
    result.room = room;
    result.walls = kLevels[room].wall_count;
    for (unsigned phase = 0; phase < 2; ++phase) {
      app.initialize(room, nullptr, false);
      if (phase) {
        for (unsigned step = 0; step < 3; ++step) {
          app.press_step();
          if (app.screen() != Screen::Playing) { app.initialize(room, nullptr, false); break; }
        }
      }
      for (unsigned direction = 0; direction < 12; ++direction) {
        const double heading = (direction + phase * 0.5) * kPi / 6;
        const int x = 180 + static_cast<int>(std::lround(70 * std::cos(heading)));
        const int y = 180 + static_cast<int>(std::lround(70 * std::sin(heading)));
        const uint64_t started = clock(context);
        if (app.tap(x, y, 0) != Action::Called) return false;
        const uint64_t elapsed = clock(context) - started;
        ++result.calls;
        result.call_total_us += elapsed;
        result.call_max_us = std::max(result.call_max_us, elapsed);
        result.max_echoes = std::max(result.max_echoes, static_cast<uint32_t>(app.echo_count()));
        for (size_t age_index = 0; age_index < sizeof(ages) / sizeof(ages[0]); ++age_index) {
          const uint64_t draw_started = clock(context);
          const FrameTimings timings = frame(ages[age_index], age_index != 0, context);
          const uint64_t draw_elapsed = clock(context) - draw_started;
          ++result.frames;
          result.frame_total_us += draw_elapsed;
          result.frame_max_us = std::max(result.frame_max_us, draw_elapsed);
          result.frames_over_40ms += draw_elapsed > 40000;
          result.render_total_us += timings.render_us;
          result.render_max_us = std::max(result.render_max_us, timings.render_us);
          result.transfer_total_us += timings.transfer_us;
          result.transfer_max_us = std::max(result.transfer_max_us, timings.transfer_us);
        }
      }
    }
    report(result, context);
  }
  return true;
}
}  // namespace echo_game
