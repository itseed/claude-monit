#include "touch.h"
#include <Arduino.h>
#include "hw_pins.h"

void touchBegin() { pinMode(TOUCH_IRQ, INPUT); }

bool touchPressed() { return digitalRead(TOUCH_IRQ) == LOW; }  // PENIRQ is active-low
