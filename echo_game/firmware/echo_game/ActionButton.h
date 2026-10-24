#pragma once

#include <stdint.h>

namespace echo_game {
// Emits one action on a stable press, with no repeat while held. A button held
// during startup must first be released; that release cannot trigger a step.
class ActionButton {
 public:
  void initialize(bool down, uint64_t now_ms) {
    raw_down_ = stable_down_ = down;
    ready_ = !down;
    changed_at_ = now_ms;
  }
  bool update(bool down, uint64_t now_ms) {
    if (down != raw_down_) { raw_down_ = down; changed_at_ = now_ms; }
    if (raw_down_ == stable_down_ || now_ms - changed_at_ < 25) return false;
    stable_down_ = raw_down_;
    if (!stable_down_) { ready_ = true; return false; }
    if (!ready_) return false;
    ready_ = false;
    return true;
  }

 private:
  bool raw_down_ = false, stable_down_ = false, ready_ = true;
  uint64_t changed_at_ = 0;
};
}  // namespace echo_game
