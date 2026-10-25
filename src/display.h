#pragma once
#include <TFT_eSPI.h>
#include "clock_fmt.h"
#include "layout.h"
#include "session_store.h"

class Display {
 public:
  Display() : spr_(&tft_) {}
  void begin();
  void renderSessions(const Session* const* v, int n, const ClockText& clk, uint32_t nowMs);
  void renderEmpty(const char* host, const char* ip, const ClockText& clk, uint32_t nowMs);
  void renderSetup(const char* apName);
  void renderMessage(const char* line1, const char* line2);
  // Index of the slot at screen point (x,y) in the last renderSessions(), or -1.
  int hitTest(int x, int y) const;

 private:
  TFT_eSPI tft_;
  TFT_eSprite spr_;
  Layout last_{};
  void drawCell(const Cell& c, const Session& s, int scale, int font, uint32_t nowMs);
  void drawStatusBar(const ClockText& clk);
  void drawFitted(const char* text, int cx, int y, int maxW, int font, uint16_t color);
};
