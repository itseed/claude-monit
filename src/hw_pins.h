#pragma once

// XPT2046 pen interrupt (active-low). Position reads do not work on this
// board, so only press/release is used.
#define TOUCH_IRQ 36

// On-board RGB LED, active-low
#define LED_R 4
#define LED_G 16
#define LED_B 17
