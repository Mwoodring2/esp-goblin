#pragma once
#include <stddef.h>
#include <stdint.h>

// POD metadata only. Never retains a driver buffer or frame payload.
struct WifiFrameEvent {
    uint64_t timestamp_us;
    uint8_t source[6];
    uint8_t destination[6];
    uint8_t bssid[6];
    uint16_t reason_code;
    uint8_t subtype;
    int8_t rssi;
    uint8_t channel;
    bool has_reason_code;
    bool protected_frame;
};

// length excludes FCS. Only complete, unfragmented, normal management headers.
bool parseWifiManagementFrame(const uint8_t* frame, size_t length, uint64_t timestamp_us,
                             int8_t rssi, uint8_t channel, WifiFrameEvent& out);
