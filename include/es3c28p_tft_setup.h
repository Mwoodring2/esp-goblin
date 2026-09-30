#pragma once

#define USER_SETUP_INFO "ESP Goblin / Hosyond ES3C28P / ILI9341V"

// Known-good ES3C28P community ports use TFT_eSPI's ILI9341_2 path.
#define ILI9341_2_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   46
#define TFT_RST  -1

#define TFT_BL 45
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#define SMOOTH_FONT

#define SPI_FREQUENCY      40000000
#define SPI_READ_FREQUENCY 20000000

#define TFT_INVERSION_ON
