#include "touch.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "hw_pins.h"
#include "layout.h"

namespace {
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
bool wasTouched = false;
uint32_t lastPress = 0;
}  // namespace

void touchBegin() {
  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSpi);
  ts.setRotation(1);
}

bool touchRead(int& x, int& y) {
  const bool touched = ts.tirqTouched() && ts.touched();
  const bool press = touched && !wasTouched && millis() - lastPress > 300;
  wasTouched = touched;
  if (!press) return false;
  lastPress = millis();
  TS_Point p = ts.getPoint();
  Serial.printf("touch raw x=%d y=%d\n", p.x, p.y);
  x = constrain(map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W), 0, SCREEN_W - 1);
  y = constrain(map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H), 0, SCREEN_H - 1);
  return true;
}
