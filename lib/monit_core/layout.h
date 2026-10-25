#pragma once
#include <stdint.h>
#include "session_store.h"

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

struct Cell {
  int16_t x, y, w, h;
};

struct Layout {
  int cols, rows, scale;
  int count;
  Cell cells[MAX_VISIBLE];
};

// Grid for 0..MAX_VISIBLE slots; the last column/row absorbs leftover pixels.
Layout computeLayout(int count);
