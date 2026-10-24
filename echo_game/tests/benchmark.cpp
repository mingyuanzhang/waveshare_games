#include "Benchmark.h"

#include <chrono>
#include <cstdio>
#include <vector>

using namespace echo_game;
struct Context { EchoApp app; std::vector<uint16_t> pixels = std::vector<uint16_t>(360 * 360); };
uint64_t clock_us(void*) {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
FrameTimings frame(uint64_t age, bool animation_only, void* context) {
  auto& state = *static_cast<Context*>(context);
  const uint64_t started = clock_us(nullptr);
  state.app.draw(state.pixels.data(), age, animation_only);
  FrameTimings timings;
  timings.render_us = clock_us(nullptr) - started;
  return timings;
}
void report(const BenchmarkResult& r, void*) {
  std::printf("[bench] ROOM,%u,%s,%u,%lu,%llu,%llu,%lu,%llu,%llu,%lu,%lu,%llu,%llu,%llu,%llu\n",
      static_cast<unsigned>(r.room + 1), kLevels[r.room].name, static_cast<unsigned>(r.walls),
      static_cast<unsigned long>(r.calls), static_cast<unsigned long long>(r.call_total_us),
      static_cast<unsigned long long>(r.call_max_us), static_cast<unsigned long>(r.frames),
      static_cast<unsigned long long>(r.frame_total_us), static_cast<unsigned long long>(r.frame_max_us),
      static_cast<unsigned long>(r.frames_over_40ms), static_cast<unsigned long>(r.max_echoes),
      static_cast<unsigned long long>(r.render_total_us), static_cast<unsigned long long>(r.render_max_us),
      static_cast<unsigned long long>(r.transfer_total_us), static_cast<unsigned long long>(r.transfer_max_us));
}
int main() {
  Context context;
  std::printf("[bench] BEGIN,native-mac,%u,25fps\n", static_cast<unsigned>(kRoomCount));
  const bool okay = run_benchmark(context.app, clock_us, frame, report, &context);
  std::puts(okay ? "[bench] END" : "[bench] ERROR,call workload failed");
  return okay ? 0 : 1;
}
