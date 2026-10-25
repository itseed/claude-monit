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
    StatusInfo st{formatClock(&t, (now / 500) % 2 == 0), {true, 31, (int)((now / 5000) % 4) * 30 + 1}, true, 3};
    if (store.count() == 0) {
      display.renderEmpty("claude-monit.local", "192.168.1.123", st, now);
    } else {
      const Session* v[MAX_VISIBLE];
      int n = store.visible(v);
      display.renderSessions(v, n, st, now);
    }
  }
}
#else
#include <ArduinoJson.h>
#include "led.h"
#include "net.h"
#include "snapshot.h"
#include "touch.h"
#include "details.h"
#include "weather_task.h"
#include <WiFi.h>

static char shownIds[MAX_VISIBLE][40];  // ids in the slots last drawn, for touch
static int shownCount = 0;
static TouchGesture gesture;
static char detailsId[40] = "";  // session shown full-screen after a long press
static uint32_t detailsSince = 0;
constexpr uint32_t DETAILS_TIMEOUT_MS = 15000;

static const Session* findSession(const char* id) {
  const Session* v[MAX_SESSIONS];
  const int n = store.visible(v, MAX_SESSIONS);
  for (int i = 0; i < n; i++)
    if (!strcmp(v[i]->id, id)) return v[i];
  return nullptr;
}

static void handleTouch(uint32_t now) {
  int x = 0, y = 0;
  const bool touched = touchSample(x, y);
  const TouchEvent ev = gesture.update(touched, x, y, now);
  if (ev == TouchEvent::None) return;
  if (detailsId[0]) {  // any touch closes the details view
    if (ev == TouchEvent::Tap) detailsId[0] = '\0';
    return;
  }
  const int i = display.hitTest(gesture.x(), gesture.y());
  if (i < 0 || i >= shownCount) return;
  if (ev == TouchEvent::Tap) {
    store.acknowledge(shownIds[i]);
  } else {
    memcpy(detailsId, shownIds[i], sizeof(detailsId));
    detailsSince = now;
  }
}

static bool onSnapshot(const char* body, size_t len) {
  static Snapshot s;  // ~1.3 KB, keep off the stack
  if (!parseSnapshot(body, len, s)) return false;
  store.applySnapshot(s, millis());
  return true;
}

static const char* stateName(State s) {
  switch (s) {
    case State::Waiting: return "waiting";
    case State::Done: return "done";
    case State::Working: return "working";
    default: return "idle";
  }
}

static String statusJson() {
  JsonDocument doc;
  doc["uptime_ms"] = millis();
  doc["led"] = (int)store.ledMode();
  JsonArray arr = doc["sessions"].to<JsonArray>();
  const Session* v[MAX_SESSIONS];
  const int n = store.visible(v, MAX_SESSIONS);
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = v[i]->id;
    o["name"] = v[i]->name;
    o["state"] = stateName(v[i]->state);
    o["age_ms"] = v[i]->ageAtReceiptMs + (millis() - v[i]->receivedAtMs);
  }
  String out;
  serializeJson(doc, out);
  return out;
}

static void updateLed(uint32_t now) {
  static LedMode mode = LedMode::Off;
  static uint32_t gen = 0, since = 0;
  const LedMode m = store.ledMode();
  if (m != mode || store.doneGeneration() != gen) {
    mode = m;
    gen = store.doneGeneration();
    since = now;
  }
  ledSet(ledColorAt(mode, now - since));
}

static StatusInfo currentStatus(uint32_t now) {
  struct tm t;
  StatusInfo st{};
  st.clk = formatClock(getLocalTime(&t, 0) ? &t : nullptr, (now / 500) % 2 == 0);
  st.weather = weatherNow();
  st.wifiUp = WiFi.status() == WL_CONNECTED;
  st.wifiBars = st.wifiUp ? wifiBars(WiFi.RSSI()) : 0;
  return st;
}

void setup() {
  Serial.begin(115200);
  display.begin();
  ledBegin();
  touchBegin();
  netOnSnapshot(onSnapshot);
  netOnStatus(statusJson);
  netBegin(display);
  weatherBegin();
}

void loop() {
  static uint32_t lastExpire = 0, lastFrame = 0;
  const uint32_t now = millis();
  netLoop();

  if (now - lastExpire >= 1000) {
    lastExpire = now;
    store.expire(now);
  }

  handleTouch(now);

  updateLed(now);

  if (now - lastFrame >= 66) {  // ~15 fps
    lastFrame = now;
    const StatusInfo st = currentStatus(now);
    const Session* detail = detailsId[0] ? findSession(detailsId) : nullptr;
    if (detailsId[0] && (!detail || now - detailsSince > DETAILS_TIMEOUT_MS)) {
      detailsId[0] = '\0';
      detail = nullptr;
    }
    if (detail) {
      display.renderDetails(*detail, st, now);
    } else if (store.count() == 0) {
      shownCount = 0;
      display.renderEmpty("claude-monit.local", netLocalIp().c_str(), st, now);
    } else {
      const Session* v[MAX_VISIBLE];
      shownCount = store.visible(v);
      for (int i = 0; i < shownCount; i++) memcpy(shownIds[i], v[i]->id, sizeof(shownIds[i]));
      display.renderSessions(v, shownCount, st, now);
    }
  }
}
#endif
