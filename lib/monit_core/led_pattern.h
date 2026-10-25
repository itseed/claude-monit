#pragma once
#include <stdint.h>
#include "session_store.h"

struct Rgb {
  uint8_t r, g, b;
};

// Colour the RGB LED should show `msInMode` after entering `mode`.
Rgb ledColorAt(LedMode mode, uint32_t msInMode);
