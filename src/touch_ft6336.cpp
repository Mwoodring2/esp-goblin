#include "touch_ft6336.h"
#include "es3c28p_board.h"
#include <Wire.h>

namespace {
constexpr uint8_t REG_TOUCH_COUNT = 0x02;
constexpr uint8_t REG_TOUCH_DATA  = 0x03;

bool readBytes(uint8_t reg, uint8_t* out, size_t count) {
    if (!out || count == 0) return false;

    Wire.beginTransmission(GoblinBoard::TOUCH_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;

    const size_t received = Wire.requestFrom(
        static_cast<uint8_t>(GoblinBoard::TOUCH_ADDR),
        static_cast<uint8_t>(count)
    );

    if (received != count) {
        while (Wire.available()) Wire.read();
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!Wire.available()) return false;
        out[i] = static_cast<uint8_t>(Wire.read());
    }
    return true;
}
}

bool goblinTouchBegin() {
    pinMode(GoblinBoard::TOUCH_RST, OUTPUT);
    digitalWrite(GoblinBoard::TOUCH_RST, LOW);
    delay(10);
    digitalWrite(GoblinBoard::TOUCH_RST, HIGH);
    delay(250);

    pinMode(GoblinBoard::TOUCH_INT, INPUT);
    Wire.begin(GoblinBoard::TOUCH_SDA, GoblinBoard::TOUCH_SCL);

    Wire.beginTransmission(GoblinBoard::TOUCH_ADDR);
    return Wire.endTransmission() == 0;
}

bool goblinTouchRead(GoblinTouchPoint& point) {
    point = {};

    uint8_t count = 0;
    if (!readBytes(REG_TOUCH_COUNT, &count, 1)) return false;
    count &= 0x0F;
    if (count == 0 || count > 2) return true;

    uint8_t d[4] = {};
    if (!readBytes(REG_TOUCH_DATA, d, sizeof(d))) return false;

    const int16_t rawX = static_cast<int16_t>(((d[0] & 0x0F) << 8) | d[1]);
    const int16_t rawY = static_cast<int16_t>(((d[2] & 0x0F) << 8) | d[3]);

    // Rotation 1: landscape 320x240.
    point.pressed = true;
    point.x = rawY;
    point.y = static_cast<int16_t>((GoblinBoard::NATIVE_W - 1) - rawX);

    if (point.x < 0) point.x = 0;
    if (point.y < 0) point.y = 0;
    if (point.x > 319) point.x = 319;
    if (point.y > 239) point.y = 239;

    return true;
}
