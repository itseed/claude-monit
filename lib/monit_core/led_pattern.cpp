#include "led_pattern.h"

static const Rgb OFF{0, 0, 0};
static const Rgb ORANGE{255, 60, 0};
static const Rgb GREEN{0, 255, 0};

Rgb ledColorAt(LedMode mode, uint32_t msInMode) {
  switch (mode) {
    case LedMode::Waiting:  // 2 Hz blink, forever
      return (msInMode % 500) < 250 ? ORANGE : OFF;
    case LedMode::Done:  // short green flash once a second, until acknowledged
      return (msInMode % 1000) < 200 ? GREEN : OFF;
    default:
      return OFF;
  }
}
