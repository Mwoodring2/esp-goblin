#pragma once

#include <stddef.h>
#include <stdint.h>

enum class AccessPointState { Unknown, Known };

struct AccessPointRecord {
    uint8_t bssid[6] = {};
    char ssid[33] = {};
    int channel = 0;
    int rssi = -127;
    int auth_mode = 0; // Arduino/ESP-IDF wifi_auth_mode_t numeric value.
    uint64_t first_seen = 0; // Monotonic milliseconds since boot.
    uint64_t last_seen = 0;
    uint32_t seen_count = 0; // Number of scans containing this BSSID.
    AccessPointState state = AccessPointState::Unknown;
};

class AccessPointInventory {
public:
    enum class MergeResult { Existing, New, Invalid, OutOfMemory };
    AccessPointInventory() = default;
    ~AccessPointInventory();
    AccessPointInventory(const AccessPointInventory&) = delete;
    AccessPointInventory& operator=(const AccessPointInventory&) = delete;
    void beginScan();
    MergeResult observe(const uint8_t* bssid, const char* ssid, int channel,
                        int rssi, int auth_mode, uint64_t now);
    const AccessPointRecord* find(const uint8_t* bssid) const;
    bool setKnown(const uint8_t* bssid, bool known);
    size_t totalCount() const { return total_; }
    size_t knownCount() const { return known_; }
    size_t unknownCount() const { return total_ - known_; }
    size_t newCount() const { return new_; }

private:
    struct Node {
        AccessPointRecord record;
        bool seen_this_scan = false;
        Node* next = nullptr;
    };
    Node* findNode(const uint8_t* bssid) const;
    Node* head_ = nullptr;
    size_t total_ = 0;
    size_t known_ = 0;
    size_t new_ = 0;
};
