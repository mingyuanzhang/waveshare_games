#include "../firmware/focus_timer/Countdown.h"

#include <assert.h>
#include <stdio.h>

int main() {
  constexpr uint64_t second = 1000000;
  using State = Countdown::State;
  Countdown timer(25 * 60);
  assert(timer.state() == State::Ready);
  assert(timer.remaining_seconds(0) == 1500);

  // Preserve sub-second time across a long pause instead of rounding it away.
  timer.toggle(10 * second);
  assert(timer.state() == State::Running);
  assert(timer.remaining_seconds(10 * second + 1) == 1500);
  timer.toggle(10 * second + 1234567);
  assert(timer.state() == State::Paused);
  const uint64_t paused = timer.remaining_us(0);
  assert(paused == 1500 * second - 1234567);
  assert(timer.remaining_us(24 * 60 * 60 * second) == paused);
  assert(!timer.choose_seconds(5));
  timer.toggle(24 * 60 * 60 * second);
  assert(timer.remaining_us(24 * 60 * 60 * second + 500000) == paused - 500000);

  // Updates may arrive late; completion must happen once and stay at zero.
  const uint64_t deadline = 24 * 60 * 60 * second + paused;
  assert(!timer.update(deadline - 1));
  assert(timer.remaining_seconds(deadline - 1) == 1);
  assert(timer.update(deadline + 10 * second));
  assert(timer.state() == State::Finished);
  assert(timer.remaining_seconds(deadline + 100 * second) == 0);
  assert(!timer.update(deadline + 100 * second));
  timer.toggle(deadline + 100 * second);
  assert(timer.state() == State::Running);
  assert(timer.remaining_seconds(deadline + 100 * second) == 1500);

  // Reset restores the preset and permits a new selection.
  timer.reset();
  assert(timer.state() == State::Ready);
  assert(timer.choose_seconds(5));
  const uint64_t long_uptime = 60ULL * 24 * 60 * 60 * second;
  timer.toggle(long_uptime);
  assert(!timer.choose_seconds(50));
  assert(timer.remaining_seconds(long_uptime + 2 * second) == 3);
  assert(timer.update(long_uptime + 5 * second));
  timer.reset();
  assert(timer.remaining_seconds(long_uptime) == 5);

  // A zero-length input is bounded to one second.
  assert(timer.choose_seconds(0));
  assert(timer.duration_us() == second);
  timer.toggle(long_uptime);
  assert(timer.update(long_uptime + second));

  // A late Pause tap must not silently restart an expired session.
  timer.reset();
  timer.toggle(long_uptime);
  timer.toggle(long_uptime + second);
  assert(timer.state() == State::Finished);
  assert(timer.remaining_seconds(long_uptime + second) == 0);
  timer.toggle(long_uptime + second + 1);
  assert(timer.state() == State::Running);
  assert(timer.remaining_seconds(long_uptime + second + 1) == 1);

  puts("Countdown tests passed: pause/resume, delayed completion, reset, presets, long uptime.");
}
