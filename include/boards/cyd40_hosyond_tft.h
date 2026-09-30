#pragma once

// ESP Goblin board profile:
// Hosyond 4.0" 320x480 CYD-style ESP32-32E board
// ST7796S display, resistive XPT2046 touch.
//
// IMPORTANT:
// This is the initial bring-up profile. Display first, then touch/SD are
// validated on the user's physical board before being enabled in firmware.

#define USER_SETUP_INFO "ESP Goblin / Hosyond CYD 4.0 / ST7796S"

#define ST7796_DRIVER

#define TFT_WIDTH  320
#define TFT_HEIGHT 480

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

#define TFT_BL   27
#define TFT_BACKLIGHT_ON HIGH

// Resistive touch controller chip select.
// Touch is intentionally not used by the first bring-up build.
#define TOUCH_CS 33

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 40000000
#endif

#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000

#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_ON
