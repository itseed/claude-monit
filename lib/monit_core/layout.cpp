#include "layout.h"

Layout computeLayout(int count) {
  if (count < 0) count = 0;
  if (count > MAX_VISIBLE) count = MAX_VISIBLE;

  Layout l{};
  l.count = count;
  if (count <= 1) {
    l.cols = 1; l.rows = 1; l.scale = 8;
  } else if (count == 2) {
    l.cols = 2; l.rows = 1; l.scale = 6;
  } else if (count <= 4) {
    l.cols = 2; l.rows = 2; l.scale = 4;
  } else {
    l.cols = 3; l.rows = 2; l.scale = 3;
  }

  const int cw = SCREEN_W / l.cols, ch = SCREEN_H / l.rows;
  for (int i = 0; i < count; i++) {
    const int c = i % l.cols, r = i / l.cols;
    Cell& cell = l.cells[i];
    cell.x = c * cw;
    cell.y = r * ch;
    cell.w = (c == l.cols - 1) ? SCREEN_W - cell.x : cw;
    cell.h = (r == l.rows - 1) ? SCREEN_H - cell.y : ch;
  }
  return l;
}
