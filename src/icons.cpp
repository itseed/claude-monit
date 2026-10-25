#include "icons.h"

namespace {
// '.' empty, 'S' sun, 'C' cloud, 'R' rain, 'B' bolt
const char* const SUN[8] = {"...S....", ".S.SS.S.", "..SSSS..", "SSSSSSSS",
                            "SSSSSSSS", "..SSSS..", ".S.SS.S.", "....S..."};
const char* const PARTLY[8] = {"..S.S...", "...SSS..", ".SSSSS..", "..SSCCC.",
                               "..SCCCCC", ".CCCCCCC", "CCCCCCCC", "........"};
const char* const CLOUD[8] = {"........", "...CCC..", "..CCCCC.", ".CCCCCCC",
                              "CCCCCCCC", "CCCCCCCC", ".CCCCCC.", "........"};
const char* const RAIN[8] = {"...CCC..", "..CCCCC.", ".CCCCCCC", "CCCCCCCC",
                             ".CCCCCC.", "........", ".R..R..R", "R..R..R."};
const char* const STORM[8] = {"...CCC..", "..CCCCC.", ".CCCCCCC", "CCCCCCCC",
                              ".CCBBCC.", "...BB...", "..BB....", "..B....."};

const char* const* gridFor(WeatherIcon i) {
  switch (i) {
    case WeatherIcon::Sun: return SUN;
    case WeatherIcon::PartlyCloudy: return PARTLY;
    case WeatherIcon::Rain: return RAIN;
    case WeatherIcon::Storm: return STORM;
    default: return CLOUD;
  }
}
}  // namespace

void drawWeatherIcon(TFT_eSprite& spr, int x, int y, int scale, WeatherIcon icon) {
  const uint16_t sun = spr.color565(0xf2, 0xc1, 0x2e);
  const uint16_t cloud = spr.color565(0xc8, 0xc8, 0xc0);
  const uint16_t rain = spr.color565(0x6a, 0x9b, 0xd8);
  const char* const* g = gridFor(icon);
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++) {
      const char p = g[r][c];
      if (p == '.') continue;
      const uint16_t col = p == 'S' || p == 'B' ? sun : p == 'R' ? rain : cloud;
      spr.fillRect(x + c * scale, y + r * scale, scale, scale, col);
    }
}

void drawWifiBars(TFT_eSprite& spr, int x, int y, bool up, int bars) {
  const uint16_t on = spr.color565(0xf0, 0xee, 0xe6);
  const uint16_t off = spr.color565(0x44, 0x44, 0x40);
  const uint16_t red = spr.color565(0xe0, 0x40, 0x30);
  for (int i = 0; i < 4; i++) {
    const int h = 3 * (i + 1);
    spr.fillRect(x + i * 4, y + 12 - h, 3, h, up ? (i < bars ? on : off) : off);
  }
  if (!up) {
    spr.drawLine(x + 6, y, x + 13, y + 7, red);
    spr.drawLine(x + 13, y, x + 6, y + 7, red);
  }
}
