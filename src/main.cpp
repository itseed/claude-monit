#include <Arduino.h>
#include <string.h>
#include "display.h"
#include "session_store.h"

Display display;
SessionStore store;

#include "anim.h"
#include "led.h"
#include "usage.h"

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
    e.ctx = (int8_t)((i * 37 + 12) % 100);  // 12, 49, 86, 23, ...
  }
  s.limits.h5 = 32;
  s.limits.d7 = 61;
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
    const int sessionPages = store.pageCount();
    st.pages = pagesWithUsage(sessionPages, store.limits());
    st.page = pager.update(now, st.pages, store.ledMode() != LedMode::Off);
    st.h5 = store.limits().h5;
    st.d7 = store.limits().d7;
    strcpy(st.h5Reset, "17:00");
    if (store.count() == 0) {
      display.renderEmpty("claude-monit.local", "192.168.1.123", st, now);
    } else if (isUsagePage(st.page, sessionPages, store.limits())) {
      const Session* v[4];
      display.renderUsage(v, store.page(0, v, 4), st, now);
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
    else pager.next(now, pagesWithUsage(store.pageCount(), store.limits()));
    return;
  }
  const Session* next = store.nextForDetails(detailsId);
  if (!next) return;
  memcpy(detailsId, next->id, sizeof(detailsId));
  detailsSince = now;
}

#ifdef MOCK_USAGE
// Mock for previewing the usage screens: real WiFi/HTTP (so /screen.bmp works)
// but fake sessions and limits; snapshots from the Mac are accepted and ignored.
static void loadMock(uint32_t nowMs) {
  static Snapshot s;
  s = Snapshot{};
  s.now = 1000000 + nowMs;
  struct M { const char* name; State st; int8_t ctx; };
  const M m[] = {{"claude-monit", State::Working, 14}, {"fuse-platform", State::Waiting, 57},
                 {"pms-extension", State::Done, 86}};
  for (const M& x : m) {
    SnapshotEntry& e = s.entries[s.count++];
    snprintf(e.id, sizeof(e.id), "mock-%d", s.count);
    strncpy(e.name, x.name, sizeof(e.name) - 1);
    strcpy(e.cwd, "~/project/mock");
    strcpy(e.tool, "Bash");
    e.state = x.st;
    e.updatedAt = s.now - 90000;
    e.ctx = x.ctx;
  }
  const time_t t = time(nullptr);
  const uint32_t base = t > 1600000000 ? (uint32_t)t : 0;
  s.limits.h5 = 32;
  s.limits.d7 = 61;
  s.limits.h5Reset = base ? base + 2 * 3600 + 15 * 60 : 0;
  s.limits.d7Reset = base ? base + 3 * 86400 + 4 * 3600 : 0;
  store.applySnapshot(s, nowMs);
}
#endif

static bool onSnapshot(const char* body, size_t len) {
#ifdef MOCK_USAGE
  (void)body;
  (void)len;
  return true;
#endif
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
  doc["page"] = pager.current();  // last shown page; the plan usage page is pages - 1
  doc["pages"] = pager.pages();
  JsonArray arr = doc["sessions"].to<JsonArray>();
  const Session* v[MAX_SESSIONS];
  const int n = store.visible(v, MAX_SESSIONS);
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = v[i]->id;
    o["name"] = v[i]->name;
    o["state"] = stateName(v[i]->state);
    o["age_ms"] = sessionAgeMs(*v[i], millis());
    if (v[i]->ctx >= 0) o["ctx"] = v[i]->ctx;
  }
  const Limits& lim = store.limits();
  if (lim.h5 >= 0 || lim.d7 >= 0) {
    JsonObject l = doc["limits"].to<JsonObject>();
    if (lim.h5 >= 0) l["h5"] = lim.h5;
    if (lim.h5Reset) l["h5_reset"] = lim.h5Reset;
    if (lim.d7 >= 0) l["d7"] = lim.d7;
    if (lim.d7Reset) l["d7_reset"] = lim.d7Reset;
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
  const Limits& lim = store.limits();
  const time_t epoch = time(nullptr);
  const int64_t nowEpoch = epoch > 1600000000 ? (int64_t)epoch : 0;  // 0 until NTP sync
  st.h5 = limitNow(lim.h5, lim.h5Reset, nowEpoch);
  st.d7 = limitNow(lim.d7, lim.d7Reset, nowEpoch);
  struct tm rt;
  if (st.h5 > 0 && lim.h5Reset && nowEpoch) {
    const time_t r = lim.h5Reset;
    localtime_r(&r, &rt);
    strftime(st.h5Reset, sizeof(st.h5Reset), "%H:%M", &rt);
    formatCountdown((int64_t)lim.h5Reset - nowEpoch, st.h5In, sizeof(st.h5In));
  }
  if (st.d7 > 0 && lim.d7Reset && nowEpoch) {
    const time_t r = lim.d7Reset;
    localtime_r(&r, &rt);
    strftime(st.d7Reset, sizeof(st.d7Reset), "%a %H:%M", &rt);
    formatCountdown((int64_t)lim.d7Reset - nowEpoch, st.d7In, sizeof(st.d7In));
  }
  return st;
}

void setup() {
  Serial.begin(115200);
  display.begin();
  ledBegin();
  touchBegin();
  netOnSnapshot(onSnapshot);
  netOnStatus(statusJson);
  netOnScreen([](Print& out) { display.writeBmp(out); }, Display::BMP_SIZE);
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
    const int sessionPages = store.pageCount();
    st.pages = pagesWithUsage(sessionPages, store.limits());  // plan usage is the last page
    st.page = pager.update(now, st.pages, store.ledMode() != LedMode::Off);
    const Session* detail = detailsId[0] ? findSession(detailsId) : nullptr;
    if (detailsId[0] && (!detail || now - detailsSince > DETAILS_TIMEOUT_MS)) {
      detailsId[0] = '\0';
      detail = nullptr;
    }
#ifdef MOCK_USAGE
    // Every 8 s: sessions -> plan usage page -> details -> desk clock.
    static int view = -1;
    static uint32_t viewSince = 0;
    if (view < 0 || now - viewSince >= 8000) {
      view = (view + 1) % 4;
      viewSince = now;
      loadMock(now);
      st = currentStatus(now);
    }
    const Session* mv[MAX_VISIBLE];
    const int mn = store.page(0, mv);
    if (view == 0) display.renderSessions(mv, mn, st, now);
    else if (view == 1) display.renderUsage(mv, mn, st, now);
    else if (view == 2) display.renderDetails(*mv[mn - 1], st, now);
    else display.renderEmpty("claude-monit.local", netLocalIp().c_str(), st, now);
    return;
#endif
    if (detail) {
      display.renderDetails(*detail, st, now);
    } else if (store.count() == 0) {
      display.renderEmpty("claude-monit.local", netLocalIp().c_str(), st, now);
    } else if (isUsagePage(st.page, sessionPages, store.limits())) {
      const Session* v[4];  // the four most urgent sessions' context
      display.renderUsage(v, store.page(0, v, 4), st, now);
    } else {
      const Session* v[MAX_VISIBLE];
      display.renderSessions(v, store.page(st.page, v), st, now);
    }
  }
}
#endif
