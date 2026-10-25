#include <Arduino.h>
#include <string.h>
#include "display.h"
#include "session_store.h"

Display display;
SessionStore store;

#include "anim.h"
#include "led.h"

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

#ifdef DEMO_MODE
// Cycles through every layout and state with fake sessions.
static const char* DEMO_NAMES[] = {"api", "web", "mobile", "docs", "claude-monit", "agent-teams#2",
                                   "pms-extension", "fuse"};
// clock (LED off) -> one done session (slow green) -> one waiting (fast orange) -> grids
static const int DEMO_COUNTS[] = {0, 1, 1, 2, 4, 6, 8};
static const int DEMO_FIRST_STATE[] = {0, 1, 0, 0, 0, 0, 0};  // index into the state cycle
static const uint32_t DEMO_STEP_MS[] = {40000, 8000, 8000, 5000, 5000, 5000, 32000};  // 8 sessions: 2 pages

static void loadDemo(int count, int firstState, uint32_t nowMs) {
  Snapshot s{};
  s.now = 1000000 + nowMs;
  const State states[] = {State::Waiting, State::Done, State::Waiting, State::Working, State::Idle, State::Waiting};
  for (int i = 0; i < count; i++) {
    SnapshotEntry& e = s.entries[s.count++];
    snprintf(e.id, sizeof(e.id), "demo-%d", i);
    strncpy(e.name, DEMO_NAMES[i], sizeof(e.name) - 1);
    e.state = states[(i + firstState) % 6];
    // demo-0 has waited 20 min (sweat), demo-5 6 min (blinking frame), demo-2 just started
    e.updatedAt = s.now - (i == 0 ? 20UL * 60000 : i == 5 ? 6UL * 60000 : 0);
  }
  store.applySnapshot(s, nowMs);
}

void setup() {
  Serial.begin(115200);
  display.begin();
  ledBegin();
}

void loop() {
  static int step = -1;
  static uint32_t lastStep = 0, lastFrame = 0;
  const uint32_t now = millis();
  if (step < 0 || now - lastStep > DEMO_STEP_MS[step]) {
    step = (step + 1) % (int)(sizeof(DEMO_COUNTS) / sizeof(DEMO_COUNTS[0]));
    lastStep = now;
    loadDemo(DEMO_COUNTS[step], DEMO_FIRST_STATE[step], now);
  }
  updateLed(now);
  if (now - lastFrame >= 66) {  // ~15 fps
    lastFrame = now;
    struct tm t {};  // fake clock: Sun 25 Oct 2026, 14:00 + uptime
    t.tm_year = 126;
    t.tm_mon = 9;
    t.tm_mday = 25;
    t.tm_wday = 0;
    t.tm_hour = 14 + (now / 3600000) % 10;
    t.tm_min = (now / 60000) % 60;
    static Pager pager;
    StatusInfo st{formatClock(&t, (now / 500) % 2 == 0), {true, 31, (int)((now / 5000) % 4) * 30 + 1}, true, 3};
    st.pages = store.pageCount();
    st.page = pager.update(now, st.pages, store.ledMode() != LedMode::Off);
    if (store.count() == 0) {
      display.renderEmpty("claude-monit.local", "192.168.1.123", st, now);
    } else {
      const Session* v[MAX_VISIBLE];
      int n = store.page(st.page, v);
      display.renderSessions(v, n, st, now);
    }
  }
}
#else
#include <ArduinoJson.h>
#include "net.h"
#include "snapshot.h"
#include "touch.h"
#include "details.h"
#include "weather_task.h"
#include <WiFi.h>

static TouchGesture gesture;
static Pager pager;
static unsigned presses = 0;
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

// Without touch positions: tap = acknowledge everything (or close details),
// double tap = next page, long press = details of the most urgent session, then the next.
static void handleTouch(uint32_t now) {
  const TouchEvent ev = gesture.update(touchPressed(), 0, 0, now);
  if (ev == TouchEvent::None) return;
  presses++;
  if (ev == TouchEvent::Tap || ev == TouchEvent::DoubleTap) {
    if (detailsId[0]) detailsId[0] = '\0';  // any tap closes the details view
    else if (ev == TouchEvent::Tap) store.acknowledgeAll();
    else pager.next(now, store.pageCount());
    return;
  }
  const Session* next = store.nextForDetails(detailsId);
  if (!next) return;
  memcpy(detailsId, next->id, sizeof(detailsId));
  detailsSince = now;
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
  doc["presses"] = presses;
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
    StatusInfo st = currentStatus(now);
    st.pages = store.pageCount();
    st.page = pager.update(now, st.pages, store.ledMode() != LedMode::Off);
    const Session* detail = detailsId[0] ? findSession(detailsId) : nullptr;
    if (detailsId[0] && (!detail || now - detailsSince > DETAILS_TIMEOUT_MS)) {
      detailsId[0] = '\0';
      detail = nullptr;
    }
    if (detail) {
      display.renderDetails(*detail, st, now);
    } else if (store.count() == 0) {
      display.renderEmpty("claude-monit.local", netLocalIp().c_str(), st, now);
    } else {
      const Session* v[MAX_VISIBLE];
      display.renderSessions(v, store.page(st.page, v), st, now);
    }
  }
}
#endif
