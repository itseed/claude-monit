#include "details.h"
#include <stdio.h>

TouchEvent TouchGesture::update(bool touched, int x, int y, uint32_t nowMs) {
  if (touched && !down_) {  // press starts
    down_ = true;
    longFired_ = false;
    downAt_ = nowMs;
    secondPress_ = pendingTap_;
    pendingTap_ = false;
    if (!secondPress_) {
      x_ = x;
      y_ = y;
    }
    return TouchEvent::None;
  }
  if (touched) {  // held
    if (!longFired_ && nowMs - downAt_ >= LONG_PRESS_MS) {
      longFired_ = true;
      secondPress_ = false;
      return TouchEvent::LongPress;
    }
    return TouchEvent::None;
  }
  if (down_) {  // released
    down_ = false;
    if (longFired_) return TouchEvent::None;
    if (nowMs - downAt_ < MIN_PRESS_MS) {  // glitch: forget it, keep any earlier pending tap
      pendingTap_ = secondPress_;
      secondPress_ = false;
      return TouchEvent::None;
    }
    if (secondPress_) {
      secondPress_ = false;
      return TouchEvent::DoubleTap;
    }
    pendingTap_ = true;
    releaseAt_ = nowMs;
    return TouchEvent::None;
  }
  if (pendingTap_ && nowMs - releaseAt_ >= DOUBLE_TAP_MS) {
    pendingTap_ = false;
    return TouchEvent::Tap;
  }
  return TouchEvent::None;
}

void formatAge(uint32_t ms, char* out, size_t n) {
  const uint32_t min = ms / 60000;
  if (min == 0) snprintf(out, n, "just now");
  else if (min < 60) snprintf(out, n, "%lu min", (unsigned long)min);
  else if (min % 60 == 0) snprintf(out, n, "%lu h", (unsigned long)(min / 60));
  else snprintf(out, n, "%lu h %lu min", (unsigned long)(min / 60), (unsigned long)(min % 60));
}
