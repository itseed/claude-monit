#include "led.h"
#include <Arduino.h>
#include "hw_pins.h"

void ledBegin() { ledSet({0, 0, 0}); }

void ledSet(Rgb c) {  // active-low
  analogWrite(LED_R, 255 - c.r);
  analogWrite(LED_G, 255 - c.g);
  analogWrite(LED_B, 255 - c.b);
}
