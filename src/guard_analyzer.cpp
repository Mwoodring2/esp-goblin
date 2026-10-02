#include "guard_analyzer.h"
#include <cstdio>
#include <cstring>

const char* guardSecurityName(int mode) {
    // Stable ESP-IDF wifi_auth_mode_t values; unlisted values remain visible numerically in events.
    switch (mode) {
        case 0: return "OPEN"; case 1: return "WEP"; case 2: return "WPA-PSK";
        case 3: return "WPA2-PSK"; case 4: return "WPA/WPA2"; case 5: return "WPA2-ENTERPRISE";
        case 6: return "WPA3-PSK"; case 7: return "WPA2/WPA3"; case 8: return "WAPI";
        case 9: return "OWE"; case 10: return "WPA3-ENT-192";
        case 11: return "WPA3-EXT"; case 12: return "WPA3-EXT-MIXED";
        case 13: return "DPP"; case 14: return "WPA3-ENTERPRISE";
        case 15: return "WPA2/WPA3-ENTERPRISE"; case 16: return "WPA-ENTERPRISE";
        default: return "OTHER";
    }
}
const char* guardAlertName(GuardAlertType type) {
    switch (type) {
        case GuardAlertType::BleTrackerCapableDevice: return "TRACKER-CAPABLE DEVICE OBSERVED";
        case GuardAlertType::BlePersistentTrackerCandidate: return "PERSISTENT TRACKER-CAPABLE NEARBY";
        case GuardAlertType::NewAccessPoint: return "NEW_AP";
        case GuardAlertType::SsidCollision: return "SSID_COLLISION";
        case GuardAlertType::TrustedSsidChanged: return "SSID_CHANGED";
        case GuardAlertType::TrustedSecurityChanged: return "SECURITY_CHANGED";
        case GuardAlertType::TrustedChannelChanged: return "CHANNEL_CHANGED (INFO)";
        case GuardAlertType::DisconnectBurst: return "UNUSUAL DISCONNECT ACTIVITY";
        default: return "NONE";
    }
}
bool guardImportant(GuardAlertType type) {
    return type == GuardAlertType::SsidCollision || type == GuardAlertType::TrustedSecurityChanged ||
        type == GuardAlertType::DisconnectBurst || type == GuardAlertType::BlePersistentTrackerCandidate;
}
namespace {
int priority(GuardAlertType type) {
    if (type == GuardAlertType::DisconnectBurst || type == GuardAlertType::BlePersistentTrackerCandidate) return 4;
    if (type == GuardAlertType::TrustedSecurityChanged) return 3;
    if (type == GuardAlertType::SsidCollision) return 2;
    return type == GuardAlertType::NewAccessPoint ? 0 : 1;
}
}

void GuardAnalyzer::beginScan() {
    // Scan-derived alerts refresh; passive observations survive a scan pause.
    size_t retained = 0;
    for (size_t i = 0; i < count_; ++i)
        if (alerts_[i].type == GuardAlertType::DisconnectBurst || guardBleAlert(alerts_[i].type)) alerts_[retained++] = alerts_[i];
    count_ = retained;
    dropped_ = 0;
}

bool GuardAnalyzer::recordDisconnectBurst(const DisconnectBurst& burst, const TrustedApStore& baseline) {
    size_t index = count_;
    for (size_t i = 0; i < count_; ++i)
        if (alerts_[i].type == GuardAlertType::DisconnectBurst && !std::memcmp(alerts_[i].wifi.bssid, burst.bssid, 6)) { index = i; break; }
    bool dismissed = false;
    size_t disconnect_count = 0, oldest_disconnect = count_;
    for (size_t i = 0; i < count_; ++i) {
        if (alerts_[i].type != GuardAlertType::DisconnectBurst) continue;
        ++disconnect_count;
        if (oldest_disconnect == count_ || alerts_[i].wifi.timestamp_us < alerts_[oldest_disconnect].wifi.timestamp_us)
            oldest_disconnect = i;
    }
    if (index < count_) dismissed = alerts_[index].dismissed && !burst.new_alert;
    else if (disconnect_count >= DisconnectCapacity) {
        index = oldest_disconnect;
        ++dropped_;
    }
    else if (count_ < Capacity) ++count_;
    else {
        index = 0;
        for (size_t i = 1; i < count_; ++i)
            if (priority(alerts_[i].type) < priority(alerts_[index].type) ||
                (priority(alerts_[i].type) == priority(alerts_[index].type) && alerts_[i].wifi.timestamp_us < alerts_[index].wifi.timestamp_us)) index = i;
        ++dropped_;
    }
    auto& alert = alerts_[index];
    alert = GuardAlert{};
    alert.type = GuardAlertType::DisconnectBurst;
    alert.dismissed = dismissed;
    std::memcpy(alert.wifi.bssid, burst.bssid, 6);
    std::memcpy(alert.wifi.source, burst.source, 6);
    alert.wifi.timestamp_us = burst.timestamp_us;
    alert.wifi.frame_count = burst.frame_count; alert.wifi.window_ms = burst.window_ms;
    alert.wifi.reason_code = burst.reason_code; alert.wifi.has_reason_code = burst.has_reason_code;
    alert.wifi.count_lower_bound = burst.count_lower_bound;
    alert.wifi.rssi = burst.rssi; alert.wifi.channel = burst.channel;
    alert.wifi.trusted_ap = baseline.isTrustedAccessPoint(burst.bssid);
    const auto* trusted = baseline.find(burst.bssid);
    if (trusted) {
        std::memcpy(alert.wifi.ssid, trusted->ssid, sizeof(alert.wifi.ssid));
        std::memcpy(alert.wifi.trusted_bssid, trusted->bssid, 6);
    }
    return true;
}
namespace {
void copy(char* out, const char* in) { std::strncpy(out, in, 32); out[32] = 0; }
}
void GuardAnalyzer::add(GuardAlertType type, const AccessPointRecord& ap,
                        const uint8_t* trusted, const char* old_value, const char* new_value) {
    for (size_t i = 0; i < count_; ++i)
        if (alerts_[i].type == type && !std::memcmp(alerts_[i].wifi.bssid, ap.bssid, 6) &&
            (!trusted || !std::memcmp(alerts_[i].wifi.trusted_bssid, trusted, 6))) return;
    size_t index = count_;
    if (count_ == Capacity) {
        index = 0;
        for (size_t i = 1; i < count_; ++i)
            if (priority(alerts_[i].type) < priority(alerts_[index].type)) index = i;
        ++dropped_;
        if (priority(type) <= priority(alerts_[index].type)) return;
    } else ++count_;
    auto& alert = alerts_[index];
    alert = GuardAlert{};
    alert.type = type;
    std::memcpy(alert.wifi.bssid, ap.bssid, 6);
    if (trusted) std::memcpy(alert.wifi.trusted_bssid, trusted, 6);
    copy(alert.wifi.ssid, ap.ssid);
    copy(alert.wifi.old_value, old_value);
    copy(alert.wifi.new_value, new_value);
    alert.wifi.rssi = ap.rssi;
    alert.wifi.channel = ap.channel;
    alert.wifi.auth_mode = ap.auth_mode;
}
void GuardAnalyzer::observe(const AccessPointRecord& ap, const TrustedApStore& baseline, bool is_new) {
    const auto* trusted = baseline.find(ap.bssid);
    if (baseline.isTrustedAccessPoint(ap.bssid)) {
        if (std::strcmp(trusted->ssid, ap.ssid))
            add(GuardAlertType::TrustedSsidChanged, ap, trusted->bssid, trusted->ssid, ap.ssid);
        char old_value[33], new_value[33];
        if (trusted->auth_mode != ap.auth_mode) {
            std::snprintf(old_value, sizeof(old_value), "%s (%d)", guardSecurityName(trusted->auth_mode), trusted->auth_mode);
            std::snprintf(new_value, sizeof(new_value), "%s (%d)", guardSecurityName(ap.auth_mode), ap.auth_mode);
            add(GuardAlertType::TrustedSecurityChanged, ap, trusted->bssid, old_value, new_value);
        }
        if (trusted->expected_channel != ap.channel) {
            std::snprintf(old_value, sizeof(old_value), "%d", trusted->expected_channel);
            std::snprintf(new_value, sizeof(new_value), "%d", ap.channel);
            add(GuardAlertType::TrustedChannelChanged, ap, trusted->bssid, old_value, new_value);
        }
    } else if (ap.ssid[0]) { // Empty hidden SSIDs are not evidence of a matching network name.
        for (size_t i = 0; i < baseline.count(); ++i) {
            const auto* candidate = baseline.at(i);
            if (candidate->enabled && !std::strcmp(candidate->ssid, ap.ssid))
                add(GuardAlertType::SsidCollision, ap, candidate->bssid);
        }
    }
    if (is_new) add(GuardAlertType::NewAccessPoint, ap, nullptr);
}
size_t GuardAnalyzer::activeCount() const {
    size_t active = 0;
    for (size_t i = 0; i < count_; ++i) if (!alerts_[i].dismissed) ++active;
    return active;
}
bool GuardAnalyzer::recordBle(const BleDeviceRecord& record, const PrivacyCandidate& candidate) {
    const auto type = candidate.persistent ? GuardAlertType::BlePersistentTrackerCandidate : GuardAlertType::BleTrackerCapableDevice;
    size_t index = count_, ble_count = 0, replace = count_;
    for (size_t i = 0; i < count_; ++i) {
        if (!guardBleAlert(alerts_[i].type)) continue;
        ++ble_count;
        if (bleSameKey(alerts_[i].ble.key, record.key)) index = i;
        if ((alerts_[i].dismissed || alerts_[i].type != GuardAlertType::BlePersistentTrackerCandidate) &&
            (replace == count_ || alerts_[i].ble.last_seen < alerts_[replace].ble.last_seen)) replace = i;
    }
    bool notify = true, dismissed = false;
    if (index < count_) {
        notify = alerts_[index].type != type;
        dismissed = !notify && alerts_[index].dismissed;
    } else if (ble_count >= 8) {
        if (replace == count_) { ++dropped_; return false; }
        index = replace; ++dropped_;
    } else if (count_ < Capacity) ++count_;
    else {
        for (size_t i = 0; i < count_; ++i)
            if (alerts_[i].dismissed || priority(alerts_[i].type) < priority(type)) { index = i; break; }
        ++dropped_; if (index == count_) return false;
    }
    auto& alert = alerts_[index]; alert = GuardAlert{}; alert.type = type; alert.dismissed = dismissed;
    alert.ble.key = record.key; alert.ble.classification = record.advertisement.classification;
    alert.ble.confidence = record.advertisement.confidence;
    alert.ble.first_seen = candidate.first_seen; alert.ble.last_seen = candidate.last_seen;
    alert.ble.seen_count = record.seen_count; alert.ble.rssi = record.rssi;
    return notify;
}
void GuardAnalyzer::expireBle(uint64_t now) {
    for (size_t i = 0; i < count_;) {
        if (guardBleAlert(alerts_[i].type) && now >= alerts_[i].ble.last_seen && now - alerts_[i].ble.last_seen >= kPrivacyExpiryMs)
            alerts_[i] = alerts_[--count_];
        else ++i;
    }
}
int GuardAnalyzer::firstImportant() const {
    int best = -1;
    for (size_t i = 0; i < count_; ++i)
        if (!alerts_[i].dismissed && guardImportant(alerts_[i].type) &&
            (best < 0 || priority(alerts_[i].type) > priority(alerts_[best].type))) best = static_cast<int>(i);
    return best;
}
