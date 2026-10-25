#include "clock_fmt.h"
#include <stdio.h>
#include <string.h>

ClockText formatClock(const struct tm* t, bool colonOn) {
  static const char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  ClockText c{};
  if (!t) {
    strcpy(c.time, "--:--");
    return c;
  }
  snprintf(c.date, sizeof(c.date), "%s %d %s", DAYS[t->tm_wday % 7], t->tm_mday, MONTHS[t->tm_mon % 12]);
  snprintf(c.time, sizeof(c.time), "%02d%c%02d", t->tm_hour % 24, colonOn ? ':' : ' ', t->tm_min % 60);
  return c;
}
