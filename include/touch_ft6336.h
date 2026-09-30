#pragma once
#include <Arduino.h>

struct GoblinTouchPoint {
    bool pressed = false;
    int16_t x = 0;
    int16_t y = 0;
};

bool goblinTouchBegin();
bool goblinTouchRead(GoblinTouchPoint& point);
