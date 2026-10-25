#pragma once
#include <TFT_eSPI.h>
#include "clock_fmt.h"
#include "layout.h"
#include "session_store.h"
#include "weather.h"

struct StatusInfo {
  ClockText clk;
  Weather weather;
  bool wifiUp;
  int wifiBars;
};

class Display {
 public:
  Display() : spr_(&tft_) {}
  void begin();
  void renderSessions(const Session* const* v, int n, const StatusInfo& st, uint32_t nowMs);
  void renderEmpty(const char* host, const char* ip, const StatusInfo& st, uint32_t nowMs);
  void renderDetails(const Session& s, const StatusInfo& st, uint32_t nowMs);
  void renderSetup(const char* apName);
  void renderMessage(const char* line1, const char* line2);

 private:
  TFT_eSPI tft_;
  TFT_eSprite spr_;
  Layout last_{};
  void drawCell(const Cell& c, const Session& s, int scale, int font, uint32_t nowMs);
  void drawStatusBar(const StatusInfo& st);
  void drawTemp(const Weather& w, int x, int y, int font, uint8_t datum);
  void drawFitted(const char* text, int cx, int y, int maxW, int font, uint16_t color);
};
