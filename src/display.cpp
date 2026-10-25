#include "display.h"
#include <string.h>
#include "clawd.h"

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
  drawClawd(spr_, sx, sy, scale, s.state, clawdFrame(s.state, nowMs), idle);

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

void Display::renderSessions(const Session* const* v, int n, uint32_t nowMs) {
  last_ = computeLayout(n);
  spr_.fillSprite(BG);
  const int font = last_.count <= 2 ? 2 : 1;
  for (int i = 0; i < last_.count; i++) drawCell(last_.cells[i], *v[i], last_.scale, font, nowMs);
  spr_.pushSprite(0, 0);
}

void Display::renderEmpty(const char* host, const char* ip, uint32_t nowMs) {
  last_ = computeLayout(0);
  spr_.fillSprite(BG);
  const int scale = 8;
  const int sx = (SCREEN_W - CLAWD_W * scale) / 2, sy = 40;
  drawClawd(spr_, sx, sy, scale, State::Idle, 0, false);
  spr_.setTextColor(IDLE_FG);
  spr_.setTextDatum(TL_DATUM);
  spr_.drawString("z", sx + CLAWD_W * scale, sy - (int)((nowMs / 500) % 4) * 4, 4);
  spr_.setTextDatum(TC_DATUM);
  spr_.drawString(host, SCREEN_W / 2, 176, 2);
  spr_.drawString(ip, SCREEN_W / 2, 196, 2);
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

int Display::hitTest(int x, int y) const {
  for (int i = 0; i < last_.count; i++) {
    const Cell& c = last_.cells[i];
    if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) return i;
  }
  return -1;
}
