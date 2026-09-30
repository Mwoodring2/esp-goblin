#pragma once

#include <Arduino.h>

enum class GoblinTouchType : uint8_t {
    None = 0,
    XPT2046,
    CST820,
    GT911
};

enum class GoblinDisplayBus : uint8_t {
    Spi = 0,
    Qspi,
    Rgb
};

struct BoardProfile {
    const char* id;
    const char* display_name;

    uint16_t native_width;
    uint16_t native_height;
    uint8_t default_rotation;

    GoblinDisplayBus display_bus;
    GoblinTouchType touch_type;

    bool has_sd;
    bool has_psram;
    bool has_rgb_led;

    int8_t sd_cs;
    int8_t sd_sck;
    int8_t sd_miso;
    int8_t sd_mosi;
};
