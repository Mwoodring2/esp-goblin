#pragma once
#include <cstddef>
#include <cstdint>
#include <atomic>

constexpr size_t kBleQueueCapacity = 64, kBleInventoryCapacity = 64, kPrivacyCandidateCapacity = 16;
constexpr uint64_t kPersistentMinimumDurationMs = 600000, kPrivacyExpiryMs = 120000;
constexpr uint32_t kPersistentMinimumWindows = 3;
enum class BleDeviceClass { Unknown, GenericBle, Beacon, IBeacon, Eddystone, GoogleFindHub, TrackerCandidate };
enum class DetectionConfidence { Low, Medium, High };
enum class BleIdentityStability { Stable, RandomAddress, Unknown };
struct BleKey { uint8_t address[6] = {}; uint8_t type = 0; };
bool bleSameKey(const BleKey& a, const BleKey& b);
struct BleObservationEvent {
    uint64_t timestamp_ms = 0;
    uint32_t window = 0;
    BleKey key;
    int8_t rssi = -127;
    uint8_t payload[64] = {}, payload_length = 0;
    bool truncated = false;
};
class BleEventQueue {
public:
    bool push(const BleObservationEvent& event);
    bool pop(BleObservationEvent& event);
    uint32_t dropped() const { return drops_.load(); }
private:
    BleObservationEvent events_[kBleQueueCapacity];
    std::atomic<uint32_t> read_{0}, write_{0}, drops_{0};
};
struct BleService { uint8_t bytes[16] = {}, length = 0; };
struct BleParsedAdvertisement {
    char name[33] = {};
    bool complete_name = false, has_company = false, has_flags = false, has_tx_power = false;
    bool malformed = false, service_overflow = false;
    uint16_t company = 0;
    uint8_t flags = 0;
    int8_t tx_power = 0;
    BleService services[4];
    uint8_t service_count = 0;
    BleDeviceClass classification = BleDeviceClass::Unknown;
    DetectionConfidence confidence = DetectionConfidence::Low;
    const char* signature = "none";
};
struct BleSignature {
    const char* identifier;
    BleDeviceClass classification;
    DetectionConfidence confidence;
    const char* source;
    const char* license;
    uint8_t ad_type;
    uint16_t service_uuid; // zero means no service UUID prefix
    bool (*matches)(const uint8_t*, size_t);
};
const BleSignature* bleSignatures(size_t& count);
void classifyBleAdStructure(BleParsedAdvertisement& parsed, uint8_t type, const uint8_t* data, size_t length);
BleParsedAdvertisement parseBleAdvertisement(const uint8_t* payload, size_t length);
const char* bleClassName(BleDeviceClass value);
const char* bleConfidenceName(DetectionConfidence value);
const char* bleCompanyName(uint16_t company);
const char* bleSignalCategory(int rssi);
struct BleDeviceRecord {
    BleKey key;
    uint64_t first_seen = 0, last_seen = 0;
    uint32_t seen_count = 0;
    int8_t rssi = -127, strongest_rssi = -127;
    BleParsedAdvertisement advertisement;
    BleIdentityStability stability = BleIdentityStability::Unknown;
    bool persistent = false;
};
class BleDeviceInventory {
public:
    BleDeviceRecord* observe(const BleObservationEvent& event, const BleParsedAdvertisement& parsed);
    BleDeviceRecord* find(const BleKey& key);
    const BleDeviceRecord* at(size_t index) const { return index < count_ ? &records_[index] : nullptr; }
    size_t count() const { return count_; }
    uint32_t evictions() const { return evictions_; }
    uint32_t drops() const { return drops_; }
    void setPersistent(const BleKey& key, bool value);
private:
    BleDeviceRecord records_[kBleInventoryCapacity];
    size_t count_ = 0;
    uint32_t evictions_ = 0, drops_ = 0;
};
struct PrivacyCandidate {
    BleKey key;
    uint64_t first_seen = 0, last_seen = 0;
    uint32_t windows = 0, last_window = 0;
    bool persistent = false;
};
class PrivacyWatchAnalyzer {
public:
    const PrivacyCandidate* observe(const BleObservationEvent& event, BleDeviceClass classification);
    void expire(uint64_t now);
    const PrivacyCandidate* find(const BleKey& key) const;
    size_t count() const { return count_; }
    uint32_t drops() const { return drops_; }
    uint32_t evictions() const { return evictions_; }
private:
    PrivacyCandidate candidates_[kPrivacyCandidateCapacity];
    size_t count_ = 0;
    uint32_t drops_ = 0, evictions_ = 0;
};
