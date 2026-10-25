#pragma once

// XPT2046 touch controller (separate SPI bus from the display)
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK 25

// Raw touch range, measured on this board (see Task 1 smoke test)
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 240
#define TOUCH_Y_MAX 3800

// On-board RGB LED, active-low
#define LED_R 4
#define LED_G 16
#define LED_B 17
