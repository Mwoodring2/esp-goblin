#pragma once
#include "trusted_ap_store.h"
#include "deauth_detector.h"
#include "ble_privacy.h"

enum class GuardAlertType { None, NewAccessPoint, SsidCollision, TrustedSsidChanged,
                            TrustedSecurityChanged, TrustedChannelChanged, DisconnectBurst, BleTrackerCapableDevice, BlePersistentTrackerCandidate };
struct WifiAlertContext {
    uint8_t bssid[6] = {};
    uint8_t trusted_bssid[6] = {};
    char ssid[33] = {};
    char old_value[33] = {};
    char new_value[33] = {};
    int rssi = -127;
    int channel = 0;
    int auth_mode = 0;
    uint8_t source[6] = {};
    uint64_t timestamp_us = 0;
    uint32_t frame_count = 0, window_ms = 0;
    uint16_t reason_code = 0;
    bool has_reason_code = false, trusted_ap = false, count_lower_bound = false;
};

struct BleAlertContext {
    BleKey key;
    BleDeviceClass classification = BleDeviceClass::Unknown;
    DetectionConfidence confidence = DetectionConfidence::Low;
    uint64_t first_seen = 0, last_seen = 0;
    uint32_t seen_count = 0;
    int rssi = -127;
};
struct GuardAlert {
    GuardAlertType type = GuardAlertType::None;
    bool dismissed = false;
    WifiAlertContext wifi;
    BleAlertContext ble;
};
inline bool guardBleAlert(GuardAlertType type) {
    return type == GuardAlertType::BleTrackerCapableDevice || type == GuardAlertType::BlePersistentTrackerCandidate;
}

const char* guardSecurityName(int mode);
const char* guardAlertName(GuardAlertType type);
bool guardImportant(GuardAlertType type);

class GuardAnalyzer {
public:
    static constexpr size_t Capacity = 32;
    static constexpr size_t DisconnectCapacity = 8;
    void beginScan();
    bool recordBle(const BleDeviceRecord& record, const PrivacyCandidate& candidate);
    void expireBle(uint64_t now);
    bool recordDisconnectBurst(const DisconnectBurst& burst, const TrustedApStore& baseline);
    void observe(const AccessPointRecord& ap, const TrustedApStore& baseline, bool is_new);
    const GuardAlert* at(size_t index) const { return index < count_ ? &alerts_[index] : nullptr; }
    size_t count() const { return count_; }
    size_t activeCount() const;
    size_t droppedCount() const { return dropped_; }
    int firstImportant() const;
    void dismiss(size_t index) { if (index < count_) alerts_[index].dismissed = true; }
private:
    void add(GuardAlertType type, const AccessPointRecord& ap, const uint8_t* trusted,
             const char* old_value = "", const char* new_value = "");
    GuardAlert alerts_[Capacity];
    size_t count_ = 0;
    size_t dropped_ = 0;
};
