#pragma once

void touchBegin();
// True once per new press (debounced 300 ms); x,y in screen coordinates.
bool touchRead(int& x, int& y);
