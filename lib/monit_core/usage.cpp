#include "usage.h"
#include <stdio.h>

int limitNow(int8_t pct, uint32_t resetEpoch, int64_t nowEpoch) {
  if (pct < 0) return -1;
  if (resetEpoch && nowEpoch > 0 && nowEpoch >= (int64_t)resetEpoch) return 0;
  return pct;
}

int usageLevel(int pct) {
  if (pct >= 80) return 2;
  if (pct >= 50) return 1;
  return 0;
}

void formatLimit(const char* name, int pct, char* buf, int cap) {
  if (cap <= 0) return;
  if (pct < 0) {
    buf[0] = '\0';
    return;
  }
  snprintf(buf, cap, "%s %d%%", name, pct);
}

void formatCountdown(int64_t secs, char* buf, int cap) {
  if (cap <= 0) return;
  if (secs < 0) {
    buf[0] = '\0';
    return;
  }
  const long m = (long)(secs / 60), h = m / 60, d = h / 24;
  if (d > 0) snprintf(buf, cap, "%ldd %ldh", d, h % 24);
  else if (h > 0) snprintf(buf, cap, "%ldh %ldm", h, m % 60);
  else if (m > 0) snprintf(buf, cap, "%ldm", m);
  else snprintf(buf, cap, "<1m");
}
