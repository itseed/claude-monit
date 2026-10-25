#pragma once

void touchBegin();
// Current touch state; when touched, x,y are screen coordinates.
bool touchSample(int& x, int& y);
