#include "details.h"
#include <stdio.h>

TouchEvent TouchGesture::update(bool touched, int x, int y, uint32_t nowMs) {
  if (touched && !down_) {
    down_ = true;
    longFired_ = false;
    downAt_ = nowMs;
    x_ = x;
    y_ = y;
    return TouchEvent::None;
  }
  if (touched) {
    if (!longFired_ && nowMs - downAt_ >= LONG_PRESS_MS) {
      longFired_ = true;
      return TouchEvent::LongPress;
    }
    return TouchEvent::None;
  }
  if (down_) {
    down_ = false;
    if (longFired_ || nowMs - downAt_ < MIN_PRESS_MS) return TouchEvent::None;
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
