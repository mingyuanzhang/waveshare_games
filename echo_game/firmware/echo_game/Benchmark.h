#pragma once

#include "EchoApp.h"

namespace echo_game {
struct FrameTimings { uint64_t render_us = 0, transfer_us = 0; };
struct BenchmarkResult {
  size_t room = 0, walls = 0;
  uint32_t calls = 0, frames = 0, max_echoes = 0, frames_over_40ms = 0;
  uint64_t call_total_us = 0, call_max_us = 0;
  uint64_t frame_total_us = 0, frame_max_us = 0;
  uint64_t render_total_us = 0, render_max_us = 0;
  uint64_t transfer_total_us = 0, transfer_max_us = 0;
};
using BenchmarkClock = uint64_t (*)(void*);
using BenchmarkFrame = FrameTimings (*)(uint64_t age_ms, bool animation_only, void*);
using BenchmarkReport = void (*)(const BenchmarkResult&, void*);

// Runs the actual touch-call and renderer paths: 24 calls and 96 frames per room,
// at the start and after moving toward a wall. Caller preserves the live game.
bool run_benchmark(EchoApp& app, BenchmarkClock clock, BenchmarkFrame frame,
                   BenchmarkReport report, void* context);
}  // namespace echo_game
