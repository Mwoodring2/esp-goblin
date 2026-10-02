#include "deauth_detector.h"
#include <cstring>

void DeauthDetector::reset() {
    for (auto& history : histories_) history = History{};
    evictions_ = 0;
}
bool DeauthDetector::observe(const WifiFrameEvent& event, DisconnectBurst& burst) {
    burst = DisconnectBurst{};
    if (event.subtype != 10 && event.subtype != 12) return false;
    uint8_t any = 0;
    for (auto byte : event.bssid) any |= byte;
    if (!any || (event.bssid[0] & 1)) return false;
    History* slot = nullptr;
    History* oldest = &histories_[0];
    for (auto& history : histories_) {
        if (history.used && !std::memcmp(history.bssid, event.bssid, 6)) { slot = &history; break; }
        if (!history.used || (oldest->used && history.last_us < oldest->last_us)) oldest = &history;
    }
    const uint64_t window = static_cast<uint64_t>(kDisconnectBurstWindowMs) * 1000;
    if (!slot) {
        slot = oldest;
        if (slot->used && event.timestamp_us >= slot->last_us && event.timestamp_us - slot->last_us <= window) ++evictions_;
        *slot = History{};
        slot->used = true;
        std::memcpy(slot->bssid, event.bssid, 6);
    }
    if (event.timestamp_us < slot->last_us) return false; // reject out-of-order input
    while (slot->count && event.timestamp_us - slot->times[slot->head] > window) {
        slot->head = (slot->head + 1) % HistoryCapacity;
        --slot->count;
    }
    if (slot->count < kDisconnectBurstThreshold) slot->alerted = false;
    if (slot->discarded && event.timestamp_us - slot->discarded_us > window) slot->discarded = false;
    if (slot->count == HistoryCapacity) {
        slot->discarded = true;
        slot->discarded_us = slot->times[slot->head];
        slot->head = (slot->head + 1) % HistoryCapacity;
        --slot->count;
    }
    slot->times[(slot->head + slot->count) % HistoryCapacity] = event.timestamp_us;
    ++slot->count;
    slot->last_us = event.timestamp_us;
    if (slot->count < kDisconnectBurstThreshold) return false;
    burst.timestamp_us = event.timestamp_us;
    std::memcpy(burst.bssid, event.bssid, 6);
    std::memcpy(burst.source, event.source, 6);
    burst.frame_count = static_cast<uint32_t>(slot->count);
    burst.rssi = event.rssi; burst.channel = event.channel;
    burst.reason_code = event.reason_code; burst.has_reason_code = event.has_reason_code;
    burst.count_lower_bound = slot->discarded;
    burst.new_alert = !slot->alerted || event.timestamp_us - slot->notified_us >= static_cast<uint64_t>(kDisconnectAlertCooldownMs) * 1000;
    if (burst.new_alert) { slot->alerted = true; slot->notified_us = event.timestamp_us; }
    return true;
}
