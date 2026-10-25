#include <Arduino.h>
#include <string.h>
#include "display.h"
#include "session_store.h"

Display display;
SessionStore store;

#ifdef DEMO_MODE
// Cycles through every layout and state with fake sessions.
static const char* DEMO_NAMES[] = {"api", "web", "mobile", "docs", "claude-monit", "agent-teams#2",
                                   "pms-extension", "fuse"};
static const int DEMO_COUNTS[] = {0, 1, 2, 4, 6, 8};

static void loadDemo(int count, uint32_t nowMs) {
  Snapshot s{};
  s.now = 1000000 + nowMs;
  const State states[] = {State::Waiting, State::Done, State::Working, State::Idle};
  for (int i = 0; i < count; i++) {
    SnapshotEntry& e = s.entries[s.count++];
    snprintf(e.id, sizeof(e.id), "demo-%d", i);
    strncpy(e.name, DEMO_NAMES[i], sizeof(e.name) - 1);
    e.state = states[i % 4];
    e.updatedAt = s.now;
  }
  store.applySnapshot(s, nowMs);
}

void setup() {
  Serial.begin(115200);
  display.begin();
}

void loop() {
  static int step = -1;
  static uint32_t lastStep = 0, lastFrame = 0;
  const uint32_t now = millis();
  if (step < 0 || now - lastStep > 5000) {
    step = (step + 1) % (int)(sizeof(DEMO_COUNTS) / sizeof(DEMO_COUNTS[0]));
    lastStep = now;
    loadDemo(DEMO_COUNTS[step], now);
  }
  if (now - lastFrame >= 66) {  // ~15 fps
    lastFrame = now;
    struct tm t {};  // fake clock: Sun 25 Oct 2026, 14:00 + uptime
    t.tm_year = 126;
    t.tm_mon = 9;
    t.tm_mday = 25;
    t.tm_wday = 0;
    t.tm_hour = 14 + (now / 3600000) % 10;
    t.tm_min = (now / 60000) % 60;
    const ClockText clk = formatClock(&t, (now / 500) % 2 == 0);
    if (store.count() == 0) {
      display.renderEmpty("claude-monit.local", "192.168.1.123", clk, now);
    } else {
      const Session* v[MAX_VISIBLE];
      int n = store.visible(v);
      display.renderSessions(v, n, clk, now);
    }
  }
}
#else
void setup() {
  Serial.begin(115200);
  display.begin();
  display.renderMessage("claude-monit", "starting...");
}

void loop() {}
#endif
