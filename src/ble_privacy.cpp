#include "ble_privacy.h"
#include <cstring>
#include <climits>

bool bleSameKey(const BleKey& a, const BleKey& b) { return a.type == b.type && !std::memcmp(a.address, b.address, 6); }
bool BleEventQueue::push(const BleObservationEvent& event) {
    const uint32_t w = write_.load(std::memory_order_relaxed);
    if (w - read_.load(std::memory_order_acquire) >= kBleQueueCapacity) { drops_.fetch_add(1); return false; }
    events_[w % kBleQueueCapacity] = event;
    write_.store(w + 1, std::memory_order_release); return true;
}
bool BleEventQueue::pop(BleObservationEvent& event) {
    const uint32_t r = read_.load(std::memory_order_relaxed);
    if (r == write_.load(std::memory_order_acquire)) return false;
    event = events_[r % kBleQueueCapacity]; read_.store(r + 1, std::memory_order_release); return true;
}
namespace {
void service(BleParsedAdvertisement& a, const uint8_t* p, uint8_t n) {
    for (uint8_t i = 0; i < a.service_count; ++i) if (a.services[i].length == n && !std::memcmp(a.services[i].bytes, p, n)) return;
    if (a.service_count == 4) { a.service_overflow = true; return; }
    auto& s = a.services[a.service_count++]; s.length = n; std::memcpy(s.bytes, p, n);
}
}

BleParsedAdvertisement parseBleAdvertisement(const uint8_t* payload, size_t length) {
    BleParsedAdvertisement a;
    if (!payload && length) { a.malformed = true; return a; }
    for (size_t offset = 0; offset < length;) {
        const size_t size = payload[offset++];
        if (!size) break;
        if (size > length - offset) { a.malformed = true; break; }
        const uint8_t type = payload[offset]; const uint8_t* p = payload + offset + 1;
        const size_t n = size - 1; offset += size;
        if (a.classification == BleDeviceClass::Unknown) a.classification = BleDeviceClass::GenericBle;
        switch (type) {
        case 0x01: if (n != 1) a.malformed = true; else { a.flags = p[0]; a.has_flags = true; } break;
        case 0x02: case 0x03: case 0x06: case 0x07: {
            const uint8_t width = type <= 3 ? 2 : 16;
            if (n % width) { a.malformed = true; break; }
            for (size_t i = 0; i < n; i += width) service(a, p + i, width);
            break;
        }
        case 0x08: case 0x09:
            if (type == 9 || !a.complete_name) {
                const size_t count = n < 32 ? n : 32;
                for (size_t i = 0; i < count; ++i) a.name[i] = p[i] >= 32 && p[i] < 127 ? static_cast<char>(p[i]) : '?';
                a.name[count] = 0; a.complete_name = type == 9;
            } break;
        case 0x0a: if (n != 1) a.malformed = true; else { a.tx_power = static_cast<int8_t>(p[0]); a.has_tx_power = true; } break;
        case 0x16: case 0x20: case 0x21: {
            const uint8_t width = type == 0x16 ? 2 : type == 0x20 ? 4 : 16;
            if (n < width) { a.malformed = true; break; }
            service(a, p, width); break;
        }
        case 0xff: if (n < 2) a.malformed = true; else if (!a.has_company) { a.company = p[0] | (uint16_t(p[1]) << 8); a.has_company = true; } break;
        default: break;
        }
        if (a.malformed) break;
        classifyBleAdStructure(a, type, p, n);
    }
    return a;
}
const char* bleClassName(BleDeviceClass c) {
    switch (c) {
    case BleDeviceClass::GenericBle: return "Generic BLE";
    case BleDeviceClass::Beacon: return "Beacon";
    case BleDeviceClass::IBeacon: return "iBeacon";
    case BleDeviceClass::Eddystone: return "Eddystone";
    case BleDeviceClass::GoogleFindHub: return "Google Find Hub";
    case BleDeviceClass::TrackerCandidate: return "Tracker-capable";
    default: return "Unknown";
    }
}
const char* bleConfidenceName(DetectionConfidence c) { return c == DetectionConfidence::High ? "HIGH" : c == DetectionConfidence::Medium ? "MEDIUM" : "LOW"; }
const char* bleCompanyName(uint16_t company) { return company == 0x004c ? "Apple" : nullptr; }
const char* bleSignalCategory(int rssi) { return rssi < -85 ? "VERY WEAK" : rssi < -75 ? "WEAK" : rssi < -65 ? "MEDIUM" : rssi < -50 ? "STRONG" : "VERY STRONG"; }
BleDeviceRecord* BleDeviceInventory::find(const BleKey& key) { for (size_t i = 0; i < count_; ++i) if (bleSameKey(records_[i].key, key)) return &records_[i]; return nullptr; }
void BleDeviceInventory::setPersistent(const BleKey& key, bool value) { auto* r = find(key); if (r) r->persistent = value; }
BleDeviceRecord* BleDeviceInventory::observe(const BleObservationEvent& e, const BleParsedAdvertisement& a) {
    auto* r = find(e.key);
    if (!r) {
        if (count_ < kBleInventoryCapacity) r = &records_[count_++];
        else {
            for (auto& candidate : records_) if (!candidate.persistent && (!r || candidate.last_seen < r->last_seen)) r = &candidate;
            if (!r) { ++drops_; return nullptr; } ++evictions_;
        }
        *r = BleDeviceRecord{}; r->key = e.key; r->first_seen = e.timestamp_ms;
        r->stability = e.key.type == 0 ? BleIdentityStability::Stable : e.key.type == 1 ? BleIdentityStability::RandomAddress : BleIdentityStability::Unknown;
    }
    if (e.timestamp_ms < r->last_seen) return r;
    r->last_seen = e.timestamp_ms; if (r->seen_count != UINT32_MAX) ++r->seen_count;
    r->rssi = e.rssi; if (e.rssi > r->strongest_rssi) r->strongest_rssi = e.rssi;
    if (a.name[0]) { std::memcpy(r->advertisement.name, a.name, sizeof(a.name)); r->advertisement.complete_name = a.complete_name; }
    if (a.has_company) { r->advertisement.has_company = true; r->advertisement.company = a.company; }
    for (uint8_t i = 0; i < a.service_count; ++i) service(r->advertisement, a.services[i].bytes, a.services[i].length);
    if (static_cast<int>(a.confidence) >= static_cast<int>(r->advertisement.confidence)) {
        r->advertisement.classification = a.classification; r->advertisement.confidence = a.confidence; r->advertisement.signature = a.signature;
    }
    r->advertisement.malformed = a.malformed; r->advertisement.service_overflow |= a.service_overflow;
    return r;
}
const PrivacyCandidate* PrivacyWatchAnalyzer::find(const BleKey& key) const { for (size_t i = 0; i < count_; ++i) if (bleSameKey(candidates_[i].key, key)) return &candidates_[i]; return nullptr; }
void PrivacyWatchAnalyzer::expire(uint64_t now) {
    for (size_t i = 0; i < count_;) {
        if (now >= candidates_[i].last_seen && now - candidates_[i].last_seen >= kPrivacyExpiryMs) candidates_[i] = candidates_[--count_];
        else ++i;
    }
}
const PrivacyCandidate* PrivacyWatchAnalyzer::observe(const BleObservationEvent& e, BleDeviceClass classification) {
    expire(e.timestamp_ms);
    if (classification != BleDeviceClass::GoogleFindHub && classification != BleDeviceClass::TrackerCandidate) return nullptr;
    PrivacyCandidate* c = nullptr;
    for (size_t i = 0; i < count_; ++i) if (bleSameKey(candidates_[i].key, e.key)) { c = &candidates_[i]; break; }
    if (!c) {
        if (count_ < kPrivacyCandidateCapacity) c = &candidates_[count_++];
        else {
            for (auto& candidate : candidates_) if (!candidate.persistent && (!c || candidate.last_seen < c->last_seen)) c = &candidate;
            if (!c) { ++drops_; return nullptr; } ++evictions_;
        }
        *c = PrivacyCandidate{}; c->key = e.key; c->first_seen = e.timestamp_ms;
    }
    if (e.timestamp_ms < c->last_seen) return c;
    c->last_seen = e.timestamp_ms;
    if (!c->windows || e.window != c->last_window) { if (c->windows != UINT32_MAX) ++c->windows; c->last_window = e.window; }
    c->persistent = c->windows >= kPersistentMinimumWindows && c->last_seen - c->first_seen >= kPersistentMinimumDurationMs;
    return c;
}
