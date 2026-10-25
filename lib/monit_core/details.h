#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr uint32_t LONG_PRESS_MS = 600;
constexpr uint32_t MIN_PRESS_MS = 40;  // shorter presses are PENIRQ glitches (GPIO36 + WiFi)

enum class TouchEvent { None, Tap, LongPress };

// Turns raw touched/not-touched samples into taps (held MIN_PRESS_MS..LONG_PRESS_MS)
// and long presses (fired once while still held).
class TouchGesture {
 public:
  TouchEvent update(bool touched, int x, int y, uint32_t nowMs);
  int x() const { return x_; }  // where the press started
  int y() const { return y_; }

 private:
  bool down_ = false;
  bool longFired_ = false;
  uint32_t downAt_ = 0;
  int x_ = 0, y_ = 0;
};

// "just now", "5 min", "2 h 10 min", "3 h".
void formatAge(uint32_t ms, char* out, size_t n);
