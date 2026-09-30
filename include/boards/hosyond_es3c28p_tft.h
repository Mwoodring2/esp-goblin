#pragma once

#define USER_SETUP_INFO "ESP Goblin / Hosyond ES3C28P / ILI9341V"

#define ILI9341_DRIVER

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

#define USE_FSPI_PORT

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 40000000
#endif

#define SPI_READ_FREQUENCY 20000000

// This IPS panel is documented and independently reported as needing inversion.
#define TFT_INVERSION_ON
