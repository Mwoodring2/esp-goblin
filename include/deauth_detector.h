#pragma once
#include "wifi_frame_event.h"
#include "air_config.h"

struct DisconnectBurst {
    uint64_t timestamp_us = 0;
    uint8_t bssid[6] = {}, source[6] = {};
    uint32_t frame_count = 0;
    uint32_t window_ms = kDisconnectBurstWindowMs;
    uint16_t reason_code = 0;
    int8_t rssi = -127;
    uint8_t channel = 0;
    bool has_reason_code = false;
    bool count_lower_bound = false;
    bool new_alert = false;
};

class DeauthDetector {
public:
    static constexpr size_t BssidCapacity = 16;
    static constexpr size_t HistoryCapacity = 64;
    bool observe(const WifiFrameEvent& event, DisconnectBurst& burst);
    void reset();
    uint32_t evictions() const { return evictions_; }
private:
    struct History {
        uint8_t bssid[6] = {};
        uint64_t times[HistoryCapacity] = {};
        uint64_t last_us = 0, notified_us = 0, discarded_us = 0;
        size_t head = 0, count = 0;
        bool used = false, alerted = false, discarded = false;
    };
    History histories_[BssidCapacity];
    uint32_t evictions_ = 0;
};
