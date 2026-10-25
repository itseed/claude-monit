// Hardware smoke test: display orientation/colors, raw touch values, RGB LED.
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "hw_pins.h"

TFT_eSPI tft;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(tft.color565(0x14, 0x14, 0x13));
  const uint16_t bars[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  for (int i = 0; i < 4; i++) tft.fillRect(i * 80, 200, 80, 40, bars[i]);
  tft.fillRect(110, 70, 100, 80, tft.color565(0xd9, 0x77, 0x57));
  tft.setTextColor(TFT_WHITE);
  tft.drawString("TL", 4, 4, 4);
  tft.drawString("R   G   B   W", 20, 180, 2);

  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSpi);
  ts.setRotation(1);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
}

void loop() {
  static uint32_t lastLed = 0;
  static int ledIdx = 0;
  if (millis() - lastLed > 1000) {
    lastLed = millis();
    const int pins[] = {LED_R, LED_G, LED_B};
    for (int i = 0; i < 3; i++) digitalWrite(pins[i], i == ledIdx ? LOW : HIGH);
    ledIdx = (ledIdx + 1) % 3;
  }
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.printf("touch raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
    tft.fillCircle(map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, 320),
                   map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 240), 3, TFT_YELLOW);
    delay(50);
  }
}
