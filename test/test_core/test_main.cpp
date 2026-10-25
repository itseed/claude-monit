#include <unity.h>
#include <string.h>
#include <string>
#include "session_store.h"
#include "snapshot.h"
#include "layout.h"
#include "led_pattern.h"
#include "clock_fmt.h"
#include "weather.h"
#include "details.h"
#include "anim.h"
#include <time.h>

static const uint32_t MIN_MS = 60UL * 1000;
static const uint32_t HOUR_MS = 60UL * MIN_MS;

void setUp() {}
void tearDown() {}

static SnapshotEntry entry(const char* id, State st, uint64_t updatedAt, const char* name = nullptr) {
  SnapshotEntry e{};
  strncpy(e.id, id, sizeof(e.id) - 1);
  strncpy(e.name, name ? name : id, sizeof(e.name) - 1);
  e.state = st;
  e.updatedAt = updatedAt;
  return e;
}

static Snapshot snap(uint64_t now, std::initializer_list<SnapshotEntry> entries) {
  Snapshot s{};
  s.now = now;
  for (const auto& e : entries) s.entries[s.count++] = e;
  return s;
}

static const Session* find(const SessionStore& store, const char* id) {
  const Session* v[MAX_SESSIONS];
  int n = store.visible(v, MAX_SESSIONS);
  for (int i = 0; i < n; i++)
    if (strcmp(v[i]->id, id) == 0) return v[i];
  return nullptr;
}

// ---- parseSnapshot ----

void test_parse_valid() {
  const char* json =
      "{\"now\":5000,\"sessions\":["
      "{\"id\":\"a\",\"name\":\"api\",\"state\":\"waiting\",\"updated_at\":4000},"
      "{\"id\":\"b\",\"name\":\"web\",\"state\":\"done\",\"updated_at\":4500}]}";
  Snapshot s;
  TEST_ASSERT_TRUE(parseSnapshot(json, strlen(json), s));
  TEST_ASSERT_EQUAL_UINT64(5000, s.now);
  TEST_ASSERT_EQUAL(2, s.count);
  TEST_ASSERT_EQUAL_STRING("a", s.entries[0].id);
  TEST_ASSERT_EQUAL_STRING("api", s.entries[0].name);
  TEST_ASSERT_TRUE(s.entries[0].state == State::Waiting);
  TEST_ASSERT_EQUAL_UINT64(4000, s.entries[0].updatedAt);
  TEST_ASSERT_TRUE(s.entries[1].state == State::Done);
}

void test_parse_rejects_garbage() {
  Snapshot s;
  const char* bad[] = {
      "not json",
      "{}",
      "{\"sessions\":[]}",
      "{\"now\":1,\"sessions\":[{\"id\":\"a\",\"name\":\"a\",\"state\":\"bogus\",\"updated_at\":1}]}",
  };
  for (const char* j : bad) TEST_ASSERT_FALSE_MESSAGE(parseSnapshot(j, strlen(j), s), j);
}

void test_parse_truncates() {
  std::string json = "{\"now\":100,\"sessions\":[";
  for (int i = 0; i < 20; i++) {
    if (i) json += ",";
    json += "{\"id\":\"s" + std::to_string(i) + "\",\"name\":\"" + std::string(60, 'x') +
            "\",\"state\":\"idle\",\"updated_at\":50}";
  }
  json += "]}";
  Snapshot s;
  TEST_ASSERT_TRUE(parseSnapshot(json.c_str(), json.size(), s));
  TEST_ASSERT_EQUAL(MAX_SESSIONS, s.count);
  TEST_ASSERT_EQUAL(sizeof(s.entries[0].name) - 1, strlen(s.entries[0].name));
}

// ---- SessionStore ----

void test_apply_replaces_set() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Idle, 900), entry("B", State::Idle, 900)}), 0);
  TEST_ASSERT_EQUAL(2, store.count());
  store.applySnapshot(snap(2000, {entry("B", State::Working, 1900)}), 10);
  TEST_ASSERT_EQUAL(1, store.count());
  TEST_ASSERT_NULL(find(store, "A"));
}

void test_older_snapshot_ignored() {
  SessionStore store;
  TEST_ASSERT_TRUE(store.applySnapshot(snap(2000, {entry("A", State::Waiting, 1900)}), 0));
  TEST_ASSERT_FALSE(store.applySnapshot(snap(1000, {entry("A", State::Working, 900)}), 5));
  TEST_ASSERT_TRUE(find(store, "A")->state == State::Waiting);
}

void test_large_clock_jump_back_resets() {
  SessionStore store;
  TEST_ASSERT_TRUE(store.applySnapshot(snap(9999999999999ull, {entry("X", State::Waiting, 1)}), 0));
  // a real Mac clock far behind the bogus one: treated as a reset, not as stale
  TEST_ASSERT_TRUE(store.applySnapshot(snap(1000000, {entry("A", State::Working, 999000)}), 10));
  TEST_ASSERT_EQUAL(1, store.count());
  TEST_ASSERT_NOT_NULL(find(store, "A"));
  TEST_ASSERT_FALSE(store.applySnapshot(snap(1000000 - STALE_WINDOW_MS + 1, {entry("A", State::Idle, 1)}), 20));
}

void test_ack_sticks_until_newer_event() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Done, 100)}), 0);
  TEST_ASSERT_TRUE(store.acknowledge("A"));
  TEST_ASSERT_TRUE(find(store, "A")->state == State::Idle);
  store.applySnapshot(snap(1100, {entry("A", State::Done, 100)}), 10);
  TEST_ASSERT_TRUE(find(store, "A")->state == State::Idle);
  store.applySnapshot(snap(1200, {entry("A", State::Waiting, 200)}), 20);
  TEST_ASSERT_TRUE(find(store, "A")->state == State::Waiting);
}

void test_ack_only_done_or_waiting() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Working, 100), entry("B", State::Waiting, 100)}), 0);
  TEST_ASSERT_FALSE(store.acknowledge("A"));
  TEST_ASSERT_FALSE(store.acknowledge("missing"));
  TEST_ASSERT_TRUE(store.acknowledge("B"));
}

void test_expire_active_30min() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Working, 1000)}), 0);
  store.expire(29 * MIN_MS);
  TEST_ASSERT_EQUAL(1, store.count());
  store.expire(31 * MIN_MS);
  TEST_ASSERT_EQUAL(0, store.count());
}

void test_acked_alert_keeps_alert_ttl() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Waiting, 1000)}), 0);
  store.acknowledge("A");
  store.expire(33 * MIN_MS);  // older than the idle TTL, but it was reported as waiting
  TEST_ASSERT_EQUAL(1, store.count());
  TEST_ASSERT_TRUE(find(store, "A")->state == State::Idle);
  store.applySnapshot(snap(1000 + 34 * MIN_MS, {entry("A", State::Waiting, 1000)}), 34 * MIN_MS);
  store.expire(35 * MIN_MS);
  TEST_ASSERT_EQUAL(1, store.count());
  TEST_ASSERT_TRUE(find(store, "A")->state == State::Idle);
}

void test_expire_alert_8h() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Waiting, 1000)}), 0);
  store.expire(7 * HOUR_MS);
  TEST_ASSERT_EQUAL(1, store.count());
  store.expire(8 * HOUR_MS + MIN_MS);
  TEST_ASSERT_EQUAL(0, store.count());
}

void test_age_uses_snapshot_clock() {
  SessionStore store;
  const uint64_t now = 10000000;
  store.applySnapshot(snap(now, {entry("A", State::Working, now - 29 * MIN_MS)}), 500);
  store.expire(500 + 2 * MIN_MS);
  TEST_ASSERT_EQUAL(0, store.count());
}

void test_expire_handles_millis_wrap() {
  SessionStore store;
  const uint32_t nearWrap = 0xFFFFFFFFu - 1000;
  store.applySnapshot(snap(1000, {entry("A", State::Working, 1000)}), nearWrap);
  store.expire(nearWrap + 5000);  // wraps past zero, only ~5 s elapsed
  TEST_ASSERT_EQUAL(1, store.count());
}

void test_visible_priority_over_6() {
  SessionStore store;
  store.applySnapshot(snap(1000, {
                                     entry("i1", State::Idle, 900),
                                     entry("w1", State::Working, 900),
                                     entry("i2", State::Idle, 900),
                                     entry("i3", State::Idle, 900),
                                     entry("w2", State::Working, 900),
                                     entry("i4", State::Idle, 900),
                                     entry("dn", State::Done, 900),
                                     entry("wt", State::Waiting, 900),
                                 }),
                      0);
  const Session* v[MAX_VISIBLE];
  int n = store.visible(v);
  TEST_ASSERT_EQUAL(6, n);
  const char* expected[] = {"i1", "w1", "i2", "w2", "dn", "wt"};
  for (int i = 0; i < 6; i++) TEST_ASSERT_EQUAL_STRING(expected[i], v[i]->id);
}

void test_visible_keeps_order_under_6() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("c", State::Waiting, 900), entry("a", State::Idle, 900),
                                  entry("b", State::Done, 900)}),
                      0);
  const Session* v[MAX_VISIBLE];
  TEST_ASSERT_EQUAL(3, store.visible(v));
  TEST_ASSERT_EQUAL_STRING("c", v[0]->id);
  TEST_ASSERT_EQUAL_STRING("a", v[1]->id);
  TEST_ASSERT_EQUAL_STRING("b", v[2]->id);
}

void test_led_mode() {
  SessionStore store;
  TEST_ASSERT_TRUE(store.ledMode() == LedMode::Off);
  store.applySnapshot(snap(1000, {entry("A", State::Done, 900)}), 0);
  TEST_ASSERT_TRUE(store.ledMode() == LedMode::Done);
  store.applySnapshot(snap(1100, {entry("A", State::Done, 900), entry("B", State::Waiting, 1000)}), 1);
  TEST_ASSERT_TRUE(store.ledMode() == LedMode::Waiting);
  store.acknowledge("B");
  TEST_ASSERT_TRUE(store.ledMode() == LedMode::Done);
  store.acknowledge("A");
  TEST_ASSERT_TRUE(store.ledMode() == LedMode::Off);
}

void test_done_generation_increments() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Idle, 900)}), 0);
  uint32_t g0 = store.doneGeneration();
  store.applySnapshot(snap(1100, {entry("A", State::Done, 1000)}), 1);
  TEST_ASSERT_EQUAL_UINT32(g0 + 1, store.doneGeneration());
  store.applySnapshot(snap(1200, {entry("A", State::Done, 1000)}), 2);
  TEST_ASSERT_EQUAL_UINT32(g0 + 1, store.doneGeneration());
}

// ---- layout ----

void test_layout_grid() {
  struct { int count, cols, rows, scale; } cases[] = {
      {0, 1, 1, 8}, {1, 1, 1, 8}, {2, 2, 1, 6}, {3, 2, 2, 4}, {4, 2, 2, 4}, {5, 3, 2, 3}, {6, 3, 2, 3},
  };
  for (auto& c : cases) {
    Layout l = computeLayout(c.count);
    TEST_ASSERT_EQUAL(c.cols, l.cols);
    TEST_ASSERT_EQUAL(c.rows, l.rows);
    TEST_ASSERT_EQUAL(c.scale, l.scale);
    TEST_ASSERT_EQUAL(c.count, l.count);
  }
}

void test_layout_below_status_bar() {
  Layout l = computeLayout(6);
  TEST_ASSERT_EQUAL(0, l.cells[0].x);
  TEST_ASSERT_EQUAL(STATUS_BAR_H, l.cells[0].y);
  TEST_ASSERT_EQUAL(106, l.cells[0].w);
  TEST_ASSERT_EQUAL(111, l.cells[0].h);
  TEST_ASSERT_EQUAL(320, l.cells[5].x + l.cells[5].w);
  TEST_ASSERT_EQUAL(STATUS_BAR_H + 111, l.cells[3].y);
  TEST_ASSERT_EQUAL(240, l.cells[5].y + l.cells[5].h);
  Layout one = computeLayout(1);
  TEST_ASSERT_EQUAL(STATUS_BAR_H, one.cells[0].y);
  TEST_ASSERT_EQUAL(240 - STATUS_BAR_H, one.cells[0].h);
}

// ---- clock ----

void test_clock_format() {
  struct tm t {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 9;  // October
  t.tm_mday = 25;
  t.tm_wday = 0;  // Sunday
  t.tm_hour = 14;
  t.tm_min = 5;
  ClockText c = formatClock(&t, true);
  TEST_ASSERT_EQUAL_STRING("Sun 25 Oct", c.date);
  TEST_ASSERT_EQUAL_STRING("14:05", c.time);
  TEST_ASSERT_EQUAL_STRING("14 05", formatClock(&t, false).time);
  t.tm_hour = 9;
  t.tm_mday = 3;
  t.tm_wday = 2;
  c = formatClock(&t, true);
  TEST_ASSERT_EQUAL_STRING("Tue 3 Oct", c.date);
  TEST_ASSERT_EQUAL_STRING("09:05", c.time);
}

void test_clock_unsynced() {
  ClockText c = formatClock(nullptr, true);
  TEST_ASSERT_EQUAL_STRING("", c.date);
  TEST_ASSERT_EQUAL_STRING("--:--", c.time);
}

// ---- led ----

static void assertRgb(Rgb want, Rgb got) {
  TEST_ASSERT_EQUAL_UINT8(want.r, got.r);
  TEST_ASSERT_EQUAL_UINT8(want.g, got.g);
  TEST_ASSERT_EQUAL_UINT8(want.b, got.b);
}

void test_led_waiting_blink() {
  assertRgb({255, 60, 0}, ledColorAt(LedMode::Waiting, 0));
  assertRgb({0, 0, 0}, ledColorAt(LedMode::Waiting, 250));
  assertRgb({255, 60, 0}, ledColorAt(LedMode::Waiting, 500));
}

void test_led_done_blinks_slowly_until_acked() {
  Rgb on{0, 255, 0}, off{0, 0, 0};
  assertRgb(on, ledColorAt(LedMode::Done, 0));
  assertRgb(on, ledColorAt(LedMode::Done, 199));
  assertRgb(off, ledColorAt(LedMode::Done, 200));
  assertRgb(off, ledColorAt(LedMode::Done, 999));
  assertRgb(on, ledColorAt(LedMode::Done, 1000));
  assertRgb(off, ledColorAt(LedMode::Done, 1500));
  assertRgb(on, ledColorAt(LedMode::Done, 3600000));  // still blinking an hour later
}

void test_led_off() {
  for (uint32_t t : {0u, 250u, 1200u, 99999u}) assertRgb({0, 0, 0}, ledColorAt(LedMode::Off, t));
}

// ---- weather / wifi ----

void test_wifi_bars() {
  TEST_ASSERT_EQUAL(4, wifiBars(-40));
  TEST_ASSERT_EQUAL(4, wifiBars(-55));
  TEST_ASSERT_EQUAL(3, wifiBars(-56));
  TEST_ASSERT_EQUAL(3, wifiBars(-65));
  TEST_ASSERT_EQUAL(2, wifiBars(-75));
  TEST_ASSERT_EQUAL(1, wifiBars(-85));
  TEST_ASSERT_EQUAL(0, wifiBars(-86));
}

void test_weather_icon_mapping() {
  TEST_ASSERT_TRUE(weatherIconFor(0) == WeatherIcon::Sun);
  TEST_ASSERT_TRUE(weatherIconFor(1) == WeatherIcon::PartlyCloudy);
  TEST_ASSERT_TRUE(weatherIconFor(2) == WeatherIcon::PartlyCloudy);
  TEST_ASSERT_TRUE(weatherIconFor(3) == WeatherIcon::Cloud);
  TEST_ASSERT_TRUE(weatherIconFor(45) == WeatherIcon::Cloud);
  TEST_ASSERT_TRUE(weatherIconFor(61) == WeatherIcon::Rain);
  TEST_ASSERT_TRUE(weatherIconFor(81) == WeatherIcon::Rain);
  TEST_ASSERT_TRUE(weatherIconFor(95) == WeatherIcon::Storm);
  TEST_ASSERT_TRUE(weatherIconFor(99) == WeatherIcon::Storm);
  TEST_ASSERT_TRUE(weatherIconFor(1234) == WeatherIcon::Cloud);
}

void test_parse_weather() {
  const char* ok = "{\"latitude\":13.75,\"current\":{\"time\":\"2026-10-25T14:00\",\"temperature_2m\":31.6,\"weather_code\":61}}";
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(ok, strlen(ok), w));
  TEST_ASSERT_TRUE(w.valid);
  TEST_ASSERT_EQUAL(32, w.tempC);
  TEST_ASSERT_EQUAL(61, w.code);
  const char* cold = "{\"current\":{\"temperature_2m\":-0.4,\"weather_code\":0}}";
  TEST_ASSERT_TRUE(parseWeather(cold, strlen(cold), w));
  TEST_ASSERT_EQUAL(0, w.tempC);
  const char* bad = "{\"error\":true}";
  Weather b{};
  TEST_ASSERT_FALSE(parseWeather(bad, strlen(bad), b));
  TEST_ASSERT_FALSE(b.valid);
}

// ---- details / gestures ----

void test_parse_optional_details() {
  const char* json =
      "{\"now\":5,\"sessions\":["
      "{\"id\":\"a\",\"name\":\"api\",\"state\":\"waiting\",\"updated_at\":4,\"cwd\":\"~/x/api\",\"tool\":\"Bash\"},"
      "{\"id\":\"b\",\"name\":\"web\",\"state\":\"idle\",\"updated_at\":4}]}";
  Snapshot s;
  TEST_ASSERT_TRUE(parseSnapshot(json, strlen(json), s));
  TEST_ASSERT_EQUAL_STRING("~/x/api", s.entries[0].cwd);
  TEST_ASSERT_EQUAL_STRING("Bash", s.entries[0].tool);
  TEST_ASSERT_EQUAL_STRING("", s.entries[1].cwd);
  TEST_ASSERT_EQUAL_STRING("", s.entries[1].tool);
  SessionStore store;
  store.applySnapshot(s, 0);
  TEST_ASSERT_EQUAL_STRING("Bash", find(store, "a")->tool);
  TEST_ASSERT_EQUAL_STRING("~/x/api", find(store, "a")->cwd);
}

void test_format_age() {
  char buf[16];
  formatAge(0, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("just now", buf);
  formatAge(59 * 1000, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("just now", buf);
  formatAge(5 * MIN_MS + 30000, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("5 min", buf);
  formatAge(2 * HOUR_MS + 10 * MIN_MS, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("2 h 10 min", buf);
  formatAge(3 * HOUR_MS, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("3 h", buf);
}

void test_gesture_tap() {
  TouchGesture g;
  TEST_ASSERT_TRUE(g.update(true, 10, 20, 1000) == TouchEvent::None);
  TEST_ASSERT_TRUE(g.update(true, 12, 22, 1200) == TouchEvent::None);
  TEST_ASSERT_TRUE(g.update(false, 0, 0, 1300) == TouchEvent::Tap);
  TEST_ASSERT_EQUAL(10, g.x());  // position where the press started
  TEST_ASSERT_EQUAL(20, g.y());
  TEST_ASSERT_TRUE(g.update(false, 0, 0, 1400) == TouchEvent::None);
}

void test_gesture_ignores_glitch() {
  TouchGesture g;
  g.update(true, 0, 0, 1000);
  TEST_ASSERT_TRUE(g.update(false, 0, 0, 1000 + MIN_PRESS_MS - 1) == TouchEvent::None);
  g.update(true, 0, 0, 2000);
  TEST_ASSERT_TRUE(g.update(false, 0, 0, 2000 + MIN_PRESS_MS) == TouchEvent::Tap);
}

void test_acknowledge_all() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("w", State::Waiting, 900), entry("d", State::Done, 900),
                                  entry("k", State::Working, 900)}),
                      0);
  TEST_ASSERT_EQUAL(2, store.acknowledgeAll());
  TEST_ASSERT_TRUE(find(store, "w")->state == State::Idle);
  TEST_ASSERT_TRUE(find(store, "d")->state == State::Idle);
  TEST_ASSERT_TRUE(find(store, "k")->state == State::Working);
  TEST_ASSERT_TRUE(store.ledMode() == LedMode::Off);
  TEST_ASSERT_EQUAL(0, store.acknowledgeAll());
}

void test_next_for_details_by_urgency() {
  SessionStore store;
  TEST_ASSERT_NULL(store.nextForDetails(""));
  store.applySnapshot(snap(1000, {entry("i", State::Idle, 990), entry("k", State::Working, 950),
                                  entry("d", State::Done, 900), entry("w", State::Waiting, 800)}),
                      0);
  const char* order[] = {"w", "d", "k", "i", "w"};  // wraps around
  const Session* s = store.nextForDetails("");
  for (const char* id : order) {
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING(id, s->id);
    s = store.nextForDetails(s->id);
  }
  TEST_ASSERT_EQUAL_STRING("w", store.nextForDetails("gone")->id);
}

void test_gesture_long_press_once() {
  TouchGesture g;
  g.update(true, 50, 60, 1000);
  TEST_ASSERT_TRUE(g.update(true, 50, 60, 1000 + LONG_PRESS_MS - 1) == TouchEvent::None);
  TEST_ASSERT_TRUE(g.update(true, 50, 60, 1000 + LONG_PRESS_MS) == TouchEvent::LongPress);
  TEST_ASSERT_EQUAL(50, g.x());
  TEST_ASSERT_TRUE(g.update(true, 50, 60, 2000) == TouchEvent::None);
  TEST_ASSERT_TRUE(g.update(false, 0, 0, 2100) == TouchEvent::None);  // no tap after long press
}

// ---- animations ----

void test_entered_at_tracks_state_changes() {
  SessionStore store;
  store.applySnapshot(snap(1000, {entry("A", State::Working, 900)}), 100);
  TEST_ASSERT_EQUAL_UINT32(100, find(store, "A")->enteredAtMs);
  store.applySnapshot(snap(1100, {entry("A", State::Working, 1000)}), 200);
  TEST_ASSERT_EQUAL_UINT32(100, find(store, "A")->enteredAtMs);  // same state: unchanged
  store.applySnapshot(snap(1200, {entry("A", State::Waiting, 1100)}), 300);
  TEST_ASSERT_EQUAL_UINT32(300, find(store, "A")->enteredAtMs);
  store.acknowledge("A");
  store.applySnapshot(snap(1300, {entry("A", State::Waiting, 1100)}), 400);  // still acked
  TEST_ASSERT_TRUE(find(store, "A")->state == State::Idle);
  store.applySnapshot(snap(1400, {entry("A", State::Done, 1350)}), 500);
  TEST_ASSERT_EQUAL_UINT32(500, find(store, "A")->enteredAtMs);
}

void test_urgency_levels() {
  TEST_ASSERT_EQUAL(0, urgencyLevel(5 * MIN_MS - 1));
  TEST_ASSERT_EQUAL(1, urgencyLevel(5 * MIN_MS));
  TEST_ASSERT_EQUAL(1, urgencyLevel(15 * MIN_MS - 1));
  TEST_ASSERT_EQUAL(2, urgencyLevel(15 * MIN_MS));
}

void test_walk_cycle() {
  const int lo = 10, hi = 250;
  WalkPose p = walkPose(0, lo, hi);
  TEST_ASSERT_TRUE(p.pose == ClawdPose::Walk);
  TEST_ASSERT_TRUE(p.facingRight);
  TEST_ASSERT_EQUAL(lo, p.x);
  TEST_ASSERT_EQUAL((lo + hi) / 2, walkPose(WALK_MS / 2, lo, hi).x);
  TEST_ASSERT_NOT_EQUAL(walkPose(0, lo, hi).frame, walkPose(250, lo, hi).frame);  // legs alternate

  p = walkPose(WALK_MS + 1000, lo, hi);
  TEST_ASSERT_TRUE(p.pose == ClawdPose::LookUp);
  TEST_ASSERT_EQUAL(hi, p.x);

  p = walkPose(WALK_MS + LOOK_MS, lo, hi);
  TEST_ASSERT_TRUE(p.pose == ClawdPose::Walk);
  TEST_ASSERT_FALSE(p.facingRight);
  TEST_ASSERT_EQUAL(hi, p.x);

  p = walkPose(2 * WALK_MS + LOOK_MS + 1000, lo, hi);
  TEST_ASSERT_TRUE(p.pose == ClawdPose::Sleep);
  TEST_ASSERT_EQUAL(lo, p.x);

  p = walkPose(WALK_CYCLE_MS, lo, hi);  // wraps
  TEST_ASSERT_TRUE(p.pose == ClawdPose::Walk);
  TEST_ASSERT_EQUAL(lo, p.x);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid);
  RUN_TEST(test_parse_rejects_garbage);
  RUN_TEST(test_parse_truncates);
  RUN_TEST(test_apply_replaces_set);
  RUN_TEST(test_older_snapshot_ignored);
  RUN_TEST(test_large_clock_jump_back_resets);
  RUN_TEST(test_ack_sticks_until_newer_event);
  RUN_TEST(test_ack_only_done_or_waiting);
  RUN_TEST(test_expire_active_30min);
  RUN_TEST(test_expire_alert_8h);
  RUN_TEST(test_acked_alert_keeps_alert_ttl);
  RUN_TEST(test_age_uses_snapshot_clock);
  RUN_TEST(test_expire_handles_millis_wrap);
  RUN_TEST(test_visible_priority_over_6);
  RUN_TEST(test_visible_keeps_order_under_6);
  RUN_TEST(test_led_mode);
  RUN_TEST(test_done_generation_increments);
  RUN_TEST(test_layout_grid);
  RUN_TEST(test_layout_below_status_bar);
  RUN_TEST(test_clock_format);
  RUN_TEST(test_clock_unsynced);
  RUN_TEST(test_led_waiting_blink);
  RUN_TEST(test_led_done_blinks_slowly_until_acked);
  RUN_TEST(test_led_off);
  RUN_TEST(test_wifi_bars);
  RUN_TEST(test_weather_icon_mapping);
  RUN_TEST(test_parse_weather);
  RUN_TEST(test_parse_optional_details);
  RUN_TEST(test_format_age);
  RUN_TEST(test_gesture_tap);
  RUN_TEST(test_gesture_long_press_once);
  RUN_TEST(test_gesture_ignores_glitch);
  RUN_TEST(test_acknowledge_all);
  RUN_TEST(test_next_for_details_by_urgency);
  RUN_TEST(test_entered_at_tracks_state_changes);
  RUN_TEST(test_urgency_levels);
  RUN_TEST(test_walk_cycle);
  return UNITY_END();
}
