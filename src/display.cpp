#include "display.h"
#include <string.h>
#include "anim.h"
#include "clawd.h"
#include "details.h"
#include "icons.h"

namespace {
constexpr uint16_t rgb(uint32_t hex) {
  return ((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex >> 3) & 0x001F);
}

constexpr uint16_t BG = rgb(0x141413);
constexpr uint16_t GRID = rgb(0x2a2a28);
constexpr uint16_t WAIT_BG = rgb(0x3a2a12);
constexpr uint16_t WAIT_FG = rgb(0xe8a33d);
constexpr uint16_t DONE_FG = rgb(0x5fb36b);
constexpr uint16_t WORK_FG = rgb(0x6a9bd8);
constexpr uint16_t IDLE_FG = rgb(0x666666);
constexpr uint16_t TEXT = rgb(0xf0eee6);

const char* stateLabel(State s) {
  switch (s) {
    case State::Waiting: return "WAITING!";
    case State::Done: return "done";
    case State::Working: return "working...";
    default: return "zzz";
  }
}

uint16_t stateColor(State s) {
  switch (s) {
    case State::Waiting: return WAIT_FG;
    case State::Done: return DONE_FG;
    case State::Working: return WORK_FG;
    default: return IDLE_FG;
  }
}
}  // namespace

void Display::begin() {
  tft_.init();
  tft_.setRotation(1);
  tft_.fillScreen(BG);
  spr_.setColorDepth(8);  // 16-bit 320x240 (150 KB) does not fit in contiguous RAM
  if (!spr_.createSprite(SCREEN_W, SCREEN_H)) Serial.println("display: sprite alloc failed");
}

void Display::drawFitted(const char* text, int cx, int y, int maxW, int font, uint16_t color) {
  char buf[48];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  if (spr_.textWidth(buf, font) > maxW) {
    size_t len = strlen(buf);
    while (len > 1) {
      buf[--len] = '\0';
      char tmp[52];
      snprintf(tmp, sizeof(tmp), "%s..", buf);
      if (spr_.textWidth(tmp, font) <= maxW) {
        strcpy(buf, tmp);
        break;
      }
    }
  }
  spr_.setTextColor(color);
  spr_.setTextDatum(TC_DATUM);
  spr_.drawString(buf, cx, y, font);
}

// One-off effects right after a state change: a "!" bubble for a new
// permission request, a pixel burst for a finished session.
void Display::drawEffects(const Cell& c, const Session& s, int sx, int sy, int scale, uint32_t nowMs) {
  const uint32_t since = nowMs - s.enteredAtMs;
  if (s.state == State::Waiting && since < 3000) {
    const int font = scale >= 4 ? 4 : 2;
    const int bw = spr_.textWidth("!", font) + 4 * scale, bh = spr_.fontHeight(font) + scale;
    int bx = sx + 12 * scale, by = sy - CLAWD_MAX_LIFT * scale - bh + ((nowMs / 150) % 2) * scale;
    if (bx + bw > c.x + c.w - 2) bx = c.x + c.w - 2 - bw;
    if (by < c.y + 2) by = c.y + 2;
    spr_.fillRoundRect(bx, by, bw, bh, scale, TEXT);
    spr_.setTextColor(WAIT_FG);
    spr_.setTextDatum(MC_DATUM);
    spr_.drawString("!", bx + bw / 2, by + bh / 2, font);
  } else if (s.state == State::Done && since < 1500) {
    static const int8_t DIRS[8][2] = {{10, 0}, {7, -7}, {0, -10}, {-7, -7}, {-10, 0}, {-7, 7}, {0, 10}, {7, 7}};
    static const uint16_t COLORS[3] = {rgb(0xf2c12e), DONE_FG, rgb(0xdb4900)};
    const int cx = sx + 8 * scale, cy = sy + 5 * scale;
    const int r = scale * (5 + (int)(since * 9 / 1500));  // radius grows outward
    for (int i = 0; i < 8; i++) {
      const int px = cx + DIRS[i][0] * r / 10, py = cy + DIRS[i][1] * r / 10;
      if (px < c.x || px + scale > c.x + c.w || py < c.y || py + scale > c.y + c.h) continue;
      spr_.fillRect(px, py, scale, scale, COLORS[(i + since / 150) % 3]);
    }
  }
}

void Display::drawCell(const Cell& c, const Session& s, int scale, int font, uint32_t nowMs) {
  if (s.state == State::Waiting) spr_.fillRect(c.x, c.y, c.w, c.h, WAIT_BG);
  spr_.drawRect(c.x, c.y, c.w, c.h, GRID);

  const int lineH = spr_.fontHeight(font) + 2;
  const int spriteH = CLAWD_H * scale;
  const int lift = CLAWD_MAX_LIFT * scale;
  const int blockH = lift + spriteH + 4 + 2 * lineH;
  const int top = c.y + (c.h - blockH) / 2;
  const int sx = c.x + (c.w - CLAWD_W * scale) / 2;
  const int sy = top + lift;
  const bool idle = s.state == State::Idle;
  const int urgency = s.state == State::Waiting ? urgencyLevel(s.ageAtReceiptMs + (nowMs - s.receivedAtMs)) : 0;
  if (s.state == State::Waiting) {  // every waiting slot gets a full orange frame
    const bool thick = urgency >= 1 && (nowMs / 250) % 2;  // waiting 5+ min: thicker, blinking
    spr_.drawRect(c.x, c.y, c.w, c.h, WAIT_FG);
    if (thick) spr_.drawRect(c.x + 1, c.y + 1, c.w - 2, c.h - 2, WAIT_FG);
  }
  drawClawd(spr_, sx, sy, scale, s.state, clawdFrame(s.state, nowMs, urgency >= 1), idle);
  if (urgency >= 2) {  // sweat drop sliding down the left side of the head
    const int dy = (nowMs / 150) % 5;
    spr_.fillRect(sx + 1 * scale, sy + (2 + dy) * scale, scale, 2 * scale, WORK_FG);
  }
  drawEffects(c, s, sx, sy, scale, nowMs);

  if (idle) {  // floating z
    const int dz = (nowMs / 500) % 4;
    spr_.setTextColor(IDLE_FG);
    spr_.setTextDatum(TL_DATUM);
    spr_.drawString("z", sx + CLAWD_W * scale - scale, sy - dz * scale, font);
  }

  const int textY = sy + spriteH + 4;
  const int cx = c.x + c.w / 2;
  drawFitted(s.name, cx, textY, c.w - 8, font, idle ? IDLE_FG : TEXT);
  const bool blinkOff = s.state == State::Waiting && (nowMs / 500) % 2;
  if (!blinkOff) drawFitted(stateLabel(s.state), cx, textY + lineH, c.w - 8, font, stateColor(s.state));
}

void Display::drawTemp(const Weather& w, int x, int y, int font, uint8_t datum) {
  // Fonts are ASCII-only, so the degree sign is drawn as a small ring.
  char t[8];
  snprintf(t, sizeof(t), "%d", w.tempC);
  const int tw = spr_.textWidth(t, font), h = spr_.fontHeight(font);
  const int ring = font >= 4 ? 3 : 2;
  const int cw = spr_.textWidth("C", font);
  const int total = tw + 2 * ring + 3 + cw;
  const int left = datum == TC_DATUM ? x - total / 2 : x;
  spr_.setTextColor(TEXT);
  spr_.setTextDatum(TL_DATUM);
  spr_.drawString(t, left, y, font);
  spr_.drawCircle(left + tw + ring + 1, y + ring + 1, ring, TEXT);
  spr_.drawString("C", left + tw + 2 * ring + 3, y, font);
  (void)h;
}

void Display::drawStatusBar(const StatusInfo& st) {
  spr_.drawFastHLine(0, STATUS_BAR_H - 1, SCREEN_W, GRID);
  spr_.setTextColor(IDLE_FG);
  spr_.setTextDatum(TL_DATUM);
  spr_.drawString(st.clk.date, 4, 1, 2);
  if (st.weather.valid) {
    drawWeatherIcon(spr_, 130, 1, 2, weatherIconFor(st.weather.code));
    drawTemp(st.weather, 150, 1, 2, TL_DATUM);
  }
  for (int i = 0; i < st.pages && st.pages > 1; i++) {  // page dots
    const int x = 214 - (st.pages - 1) * 4 + i * 8;
    if (i == st.page) spr_.fillCircle(x, 8, 2, TEXT);
    else spr_.drawCircle(x, 8, 2, IDLE_FG);
  }
  drawWifiBars(spr_, 238, 2, st.wifiUp, st.wifiBars);
  spr_.setTextColor(TEXT);
  spr_.setTextDatum(TR_DATUM);
  spr_.drawString(st.clk.time, SCREEN_W - 4, 1, 2);
}

void Display::renderSessions(const Session* const* v, int n, const StatusInfo& st, uint32_t nowMs) {
  last_ = computeLayout(n);
  spr_.fillSprite(BG);
  drawStatusBar(st);
  const int font = last_.count <= 2 ? 2 : 1;
  for (int i = 0; i < last_.count; i++) drawCell(last_.cells[i], *v[i], last_.scale, font, nowMs);
  spr_.pushSprite(0, 0);
}

void Display::renderEmpty(const char* host, const char* ip, const StatusInfo& st, uint32_t nowMs) {
  last_ = computeLayout(0);
  spr_.fillSprite(BG);
  const ClockText& clk = st.clk;
  drawWifiBars(spr_, SCREEN_W - 20, 4, st.wifiUp, st.wifiBars);

  // Clawd strolls above a big desk clock, looks up at it, then naps.
  const int scale = 4, sy = 14 + scale;
  const WalkPose w = walkPose(nowMs, 8, SCREEN_W - 28 - CLAWD_W * scale);
  drawClawdPose(spr_, w.x, sy, scale, w.pose, w.facingRight, w.frame);
  if (w.pose == ClawdPose::Sleep) {
    spr_.setTextColor(IDLE_FG);
    spr_.setTextDatum(TL_DATUM);
    spr_.drawString("z", w.x + CLAWD_W * scale, sy - (int)((nowMs / 500) % 4) * 3, 2);
  }

  // Font 7 (7-segment): draw HH, colon and MM separately so blinking never shifts digits.
  const int clockY = 84;
  spr_.setTextColor(TEXT);
  if (clk.time[2] == ':' || clk.time[2] == ' ') {
    char hh[3] = {clk.time[0], clk.time[1], 0}, mm[3] = {clk.time[3], clk.time[4], 0};
    const int digitsW = spr_.textWidth("00", 7), colonW = spr_.textWidth(":", 7);
    const int x0 = (SCREEN_W - (2 * digitsW + colonW)) / 2;
    spr_.setTextDatum(TL_DATUM);
    spr_.drawString(hh, x0, clockY, 7);
    if (clk.time[2] == ':') spr_.drawString(":", x0 + digitsW, clockY, 7);
    spr_.drawString(mm, x0 + digitsW + colonW, clockY, 7);
  } else {
    spr_.setTextDatum(TC_DATUM);
    spr_.drawString(clk.time, SCREEN_W / 2, clockY, 7);
  }

  spr_.setTextDatum(TC_DATUM);
  spr_.setTextColor(WAIT_FG);
  spr_.drawString(clk.date, SCREEN_W / 2, 140, 4);
  if (st.weather.valid) {
    char t[8];
    snprintf(t, sizeof(t), "%d", st.weather.tempC);
    const int iconW = 8 * 3, gap = 8;
    const int tempW = spr_.textWidth(t, 4) + 9 + spr_.textWidth("C", 4);
    const int x0 = (SCREEN_W - (iconW + gap + tempW)) / 2;
    drawWeatherIcon(spr_, x0, 172, 3, weatherIconFor(st.weather.code));
    drawTemp(st.weather, x0 + iconW + gap, 172, 4, TL_DATUM);
  }
  char where[64];
  snprintf(where, sizeof(where), "%s  %s", host, ip);
  spr_.setTextDatum(TC_DATUM);
  spr_.setTextColor(IDLE_FG);
  spr_.drawString(where, SCREEN_W / 2, 222, 2);
  spr_.pushSprite(0, 0);
}

void Display::renderDetails(const Session& s, const StatusInfo& st, uint32_t nowMs) {
  last_ = computeLayout(0);
  spr_.fillSprite(BG);
  if (s.state == State::Waiting) spr_.fillRect(0, STATUS_BAR_H, SCREEN_W, SCREEN_H - STATUS_BAR_H, WAIT_BG);
  drawStatusBar(st);

  const int scale = 6;
  const int sx = 14, sy = STATUS_BAR_H + 30 + CLAWD_MAX_LIFT * scale;
  drawClawd(spr_, sx, sy, scale, s.state, clawdFrame(s.state, nowMs), s.state == State::Idle);

  const int tx = sx + CLAWD_W * scale + 14, tw = SCREEN_W - tx - 6;
  const int cx = tx + tw / 2;
  char age[16], line[48];
  formatAge(s.ageAtReceiptMs + (nowMs - s.receivedAtMs), age, sizeof(age));
  switch (s.state) {
    case State::Waiting: snprintf(line, sizeof(line), "waiting %s", age); break;
    case State::Done: snprintf(line, sizeof(line), "done %s%s", age, strcmp(age, "just now") ? " ago" : ""); break;
    case State::Working: snprintf(line, sizeof(line), "working %s", age); break;
    default: snprintf(line, sizeof(line), "idle %s", age); break;
  }

  int y = STATUS_BAR_H + 36;
  drawFitted(s.name, cx, y, tw, 4, TEXT);
  y += 34;
  drawFitted(line, cx, y, tw, 2, stateColor(s.state));
  y += 24;
  if (s.state == State::Waiting && s.tool[0]) {
    snprintf(line, sizeof(line), "wants: %s", s.tool);
    drawFitted(line, cx, y, tw, 2, WAIT_FG);
    y += 24;
  }
  if (s.cwd[0]) drawFitted(s.cwd, cx, y, tw, 1, IDLE_FG);

  spr_.setTextDatum(BC_DATUM);
  spr_.setTextColor(IDLE_FG);
  spr_.drawString("tap: close    hold: next session", SCREEN_W / 2, SCREEN_H - 4, 1);
  spr_.pushSprite(0, 0);
}

void Display::renderSetup(const char* apName) {
  last_ = computeLayout(0);
  spr_.fillSprite(BG);
  drawClawd(spr_, (SCREEN_W - CLAWD_W * 5) / 2, 24, 5, State::Waiting, 0, false);
  spr_.setTextDatum(TC_DATUM);
  spr_.setTextColor(WAIT_FG);
  spr_.drawString("WiFi setup", SCREEN_W / 2, 110, 4);
  spr_.setTextColor(TEXT);
  spr_.drawString("1. On your phone, join WiFi:", SCREEN_W / 2, 150, 2);
  spr_.setTextColor(WAIT_FG);
  spr_.drawString(apName, SCREEN_W / 2, 170, 2);
  spr_.setTextColor(TEXT);
  spr_.drawString("2. Pick your network + password", SCREEN_W / 2, 196, 2);
  spr_.pushSprite(0, 0);
}

void Display::renderMessage(const char* line1, const char* line2) {
  last_ = computeLayout(0);
  spr_.fillSprite(BG);
  spr_.setTextDatum(TC_DATUM);
  spr_.setTextColor(TEXT);
  spr_.drawString(line1, SCREEN_W / 2, 90, 4);
  spr_.setTextColor(IDLE_FG);
  spr_.drawString(line2, SCREEN_W / 2, 130, 2);
  spr_.pushSprite(0, 0);
}
