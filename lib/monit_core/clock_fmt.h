#pragma once
#include <time.h>

struct ClockText {
  char date[16];  // "Sun 25 Oct", empty until time is synced
  char time[6];   // "14:05", "14 05" when the colon blinks off, "--:--" if unsynced
};

ClockText formatClock(const struct tm* t, bool colonOn);
