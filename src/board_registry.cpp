#include "board_registry.h"

#if defined(ESP_GOBLIN_BOARD_CYD40_HOSYOND)

static const BoardProfile kBoard = {
    "cyd40_hosyond",
    "Hosyond CYD 4.0 / ESP32-32E / ST7796S",
    320,
    480,
    1,
    GoblinDisplayBus::Spi,
    GoblinTouchType::XPT2046,
    true,
    false,
    false,
    5,
    18,
    19,
    23
};

#else

static const BoardProfile kBoard = {
    "unsupported",
    "Unsupported board",
    0,
    0,
    0,
    GoblinDisplayBus::Spi,
    GoblinTouchType::None,
    false,
    false,
    false,
    -1,
    -1,
    -1,
    -1
};

#endif

const BoardProfile& goblinBoard() {
    return kBoard;
}

bool goblinBoardIsSupported() {
    return kBoard.native_width > 0 && kBoard.native_height > 0;
}
