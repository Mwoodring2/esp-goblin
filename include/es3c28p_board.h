#pragma once
#include <Arduino.h>

namespace GoblinBoard {
static constexpr const char* ID = "hosyond_es3c28p";
static constexpr const char* NAME = "Hosyond ES3C28P 2.8 ESP32-S3";

static constexpr int LCD_CS   = 10;
static constexpr int LCD_DC   = 46;
static constexpr int LCD_SCLK = 12;
static constexpr int LCD_MOSI = 11;
static constexpr int LCD_MISO = 13;
static constexpr int LCD_BL   = 45;

static constexpr int TOUCH_SDA = 16;
static constexpr int TOUCH_SCL = 15;
static constexpr int TOUCH_RST = 18;
static constexpr int TOUCH_INT = 17;
static constexpr uint8_t TOUCH_ADDR = 0x38;

static constexpr int RGB_LED = 42;
static constexpr int BATTERY_ADC = 9;
static constexpr int BOOT_BUTTON = 0;

static constexpr int SD_CLK = 38;
static constexpr int SD_CMD = 40;
static constexpr int SD_D0  = 39;
static constexpr int SD_D1  = 41;
static constexpr int SD_D2  = 48;
static constexpr int SD_D3  = 47;

static constexpr uint16_t NATIVE_W = 240;
static constexpr uint16_t NATIVE_H = 320;
static constexpr uint8_t ROTATION = 1;
}
