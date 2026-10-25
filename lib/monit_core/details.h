#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr uint32_t LONG_PRESS_MS = 600;
constexpr uint32_t MIN_PRESS_MS = 40;
constexpr uint32_t DOUBLE_TAP_MS = 300;  // a tap is reported once no second tap follows  // shorter presses are PENIRQ glitches (GPIO36 + WiFi)

enum class TouchEvent { None, Tap, DoubleTap, LongPress };

// Turns raw touched/not-touched samples into taps (held MIN_PRESS_MS..LONG_PRESS_MS,
// reported DOUBLE_TAP_MS after release), double taps and long presses (fired once
// while still held).
class TouchGesture {
 public:
  TouchEvent update(bool touched, int x, int y, uint32_t nowMs);
  int x() const { return x_; }  // where the press started
  int y() const { return y_; }

 private:
  bool down_ = false;
  bool longFired_ = false;
  bool pendingTap_ = false;   // released tap waiting to see if a second one follows
  bool secondPress_ = false;  // current press started while a tap was pending
  uint32_t downAt_ = 0;
  uint32_t releaseAt_ = 0;
  int x_ = 0, y_ = 0;
};

// "just now", "5 min", "2 h 10 min", "3 h".
void formatAge(uint32_t ms, char* out, size_t n);
