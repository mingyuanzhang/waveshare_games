#pragma once

#include <stdint.h>

// Time is supplied by the caller, so wall-clock changes cannot affect a session.
class Countdown {
 public:
  enum class State { Ready, Running, Paused, Finished };

  explicit Countdown(uint32_t seconds = 25 * 60) {
    choose_seconds(seconds);
  }

  State state() const { return state_; }
  uint64_t duration_us() const { return duration_us_; }

  uint64_t remaining_us(uint64_t now_us) const {
    if (state_ == State::Running) {
      return now_us < deadline_us_ ? deadline_us_ - now_us : 0;
    }
    return remaining_us_;
  }

  uint32_t remaining_seconds(uint64_t now_us) const {
    // Round up so a just-started 25 minute session displays 25:00.
    return static_cast<uint32_t>((remaining_us(now_us) + 999999) / 1000000);
  }

  bool choose_seconds(uint32_t seconds) {
    if (state_ == State::Running || state_ == State::Paused) return false;
    duration_us_ = static_cast<uint64_t>(seconds ? seconds : 1) * 1000000;
    reset();
    return true;
  }

  bool update(uint64_t now_us) {
    if (state_ != State::Running || now_us < deadline_us_) return false;
    remaining_us_ = 0;
    state_ = State::Finished;
    return true;
  }

  void toggle(uint64_t now_us) {
    // A Pause tap that arrives after the deadline should reveal completion.
    // A separate tap on Again starts the next session.
    if (update(now_us)) return;
    if (state_ == State::Running) {
      remaining_us_ = remaining_us(now_us);
      state_ = State::Paused;
      return;
    }
    if (state_ == State::Finished) reset();
    deadline_us_ = now_us + remaining_us_;
    state_ = State::Running;
  }

  void reset() {
    remaining_us_ = duration_us_;
    deadline_us_ = 0;
    state_ = State::Ready;
  }

 private:
  State state_ = State::Ready;
  uint64_t duration_us_ = 0;
  uint64_t remaining_us_ = 0;
  uint64_t deadline_us_ = 0;
};

