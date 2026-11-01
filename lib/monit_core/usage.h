#pragma once
#include <stdint.h>
#include "session_store.h"

// Limit % to show now: 0 once its reset time has passed (when the board's
// clock is synced, nowEpoch > 0), the reported % otherwise, -1 if unknown.
int limitNow(int8_t pct, uint32_t resetEpoch, int64_t nowEpoch);

// Colour band for a usage %: 0 = calm (<50), 1 = warm (50..79), 2 = hot (80+).
int usageLevel(int pct);

// "5h 32%" style label into buf; empty string when pct < 0.
void formatLimit(const char* name, int pct, char* buf, int cap);

// Time left until a reset: "2d 4h", "2h 15m", "45m", "<1m"; "" if secs < 0.
void formatCountdown(int64_t secs, char* buf, int cap);

// The plan usage page is one extra page after the session pages, shown only
// once some limit is known. Pages = session pages (+1 for usage).
inline bool hasLimits(const Limits& l) { return l.h5 >= 0 || l.d7 >= 0; }
inline int pagesWithUsage(int sessionPages, const Limits& l) { return sessionPages + (hasLimits(l) ? 1 : 0); }
inline bool isUsagePage(int page, int sessionPages, const Limits& l) {
  return hasLimits(l) && page == sessionPages;
}
