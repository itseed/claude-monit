#pragma once
#include <TFT_eSPI.h>
#include "weather.h"

// 8x8 pixel-art weather icon, top-left at x,y, each pixel scale x scale.
void drawWeatherIcon(TFT_eSprite& spr, int x, int y, int scale, WeatherIcon icon);
// 4 signal bars in a 14x12 box (bottom-left at x,y+12); red cross when down.
void drawWifiBars(TFT_eSprite& spr, int x, int y, bool up, int bars);
