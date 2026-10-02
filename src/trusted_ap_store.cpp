#include "trusted_ap_store.h"
#include <cstring>

namespace {
uint32_t checksum(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        // CRC covers the header and payload, excluding the CRC field itself.
        if (i >= 8 && i < 12) continue;
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
bool validBssid(const uint8_t* bssid) {
    if (!bssid || (bssid[0] & 1)) return false;
    uint8_t any = 0;
    for (int i = 0; i < 6; ++i) any |= bssid[i];
    return any != 0;
}
bool terminated(const char* text) { return std::memchr(text, 0, 33) != nullptr; }
}

const TrustedAccessPoint* TrustedApStore::find(const uint8_t* bssid) const {
    if (!bssid) return nullptr;
    for (size_t i = 0; i < count_; ++i)
        if (std::memcmp(records_[i].bssid, bssid, 6) == 0) return &records_[i];
    return nullptr;
}

bool TrustedApStore::isTrustedAccessPoint(const uint8_t* bssid) const {
    const auto* record = find(bssid);
    return ready_ && record && record->enabled;
}

bool TrustedApStore::load() {
    count_ = 0;
    ready_ = false;
    size_t size = 0;
    const auto result = storage_.read(blob_, sizeof(blob_), size);
    if (result == TrustedApStorage::ReadResult::Missing) {
        ready_ = true;
        error_ = Error::None;
        return true;
    }
    if (result != TrustedApStorage::ReadResult::Ok) { error_ = Error::Unavailable; return false; }
    error_ = Error::Corrupt;
    if (size < 12 || size > sizeof(blob_) || std::memcmp(blob_, "GGRD", 4)) return false;
    if (blob_[4] != 1 || blob_[5] != 0) { error_ = Error::Incompatible; return false; }
    const size_t count = blob_[6] | (static_cast<size_t>(blob_[7]) << 8);
    if (count > Capacity || size != 12 + count * 76) return false;
    uint32_t stored_crc = 0;
    for (int i = 0; i < 4; ++i) stored_crc |= static_cast<uint32_t>(blob_[8 + i]) << (8 * i);
    if (stored_crc != checksum(blob_, size)) return false;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* p = blob_ + 12 + i * 76;
        TrustedAccessPoint record;
        std::memcpy(record.bssid, p, 6);
        std::memcpy(record.ssid, p + 6, 33);
        record.auth_mode = p[39] | (static_cast<int>(p[40]) << 8);
        record.expected_channel = p[41];
        std::memcpy(record.friendly_name, p + 42, 33);
        record.enabled = p[75] == 1;
        if (!validBssid(record.bssid) || !terminated(record.ssid) ||
            !terminated(record.friendly_name) || record.expected_channel < 1 ||
            record.expected_channel > 14 || p[75] > 1) return false;
        for (size_t j = 0; j < i; ++j)
            if (!std::memcmp(records_[j].bssid, record.bssid, 6)) return false;
        records_[i] = record;
    }
    count_ = count; // Publish only after the complete blob passes validation.
    ready_ = true;
    error_ = Error::None;
    return true;
}

bool TrustedApStore::save() {
    std::memset(blob_, 0, sizeof(blob_));
    std::memcpy(blob_, "GGRD", 4);
    blob_[4] = 1;
    blob_[6] = static_cast<uint8_t>(count_);
    for (size_t i = 0; i < count_; ++i) {
        const auto& r = records_[i];
        uint8_t* p = blob_ + 12 + i * 76;
        std::memcpy(p, r.bssid, 6);
        std::memcpy(p + 6, r.ssid, 33);
        p[39] = static_cast<uint8_t>(r.auth_mode);
        p[40] = static_cast<uint8_t>(r.auth_mode >> 8);
        p[41] = static_cast<uint8_t>(r.expected_channel);
        std::memcpy(p + 42, r.friendly_name, 33);
        p[75] = r.enabled ? 1 : 0;
    }
    const size_t size = 12 + count_ * 76;
    const uint32_t crc = checksum(blob_, size);
    for (int i = 0; i < 4; ++i) blob_[8 + i] = static_cast<uint8_t>(crc >> (8 * i));
    if (!storage_.write(blob_, size)) { error_ = Error::SaveFailed; return false; }
    error_ = Error::None;
    return true;
}

bool TrustedApStore::trust(const AccessPointRecord& observed) {
    if (!ready_) return false;
    if (!validBssid(observed.bssid) || !terminated(observed.ssid) || observed.channel < 1 ||
        observed.channel > 14 || observed.auth_mode < 0 || observed.auth_mode > 65535) {
        error_ = Error::Invalid; return false;
    }
    const auto* existing = find(observed.bssid);
    if (existing && existing->enabled) { error_ = Error::None; return true; }
    if (!existing && count_ == Capacity) { error_ = Error::Full; return false; }
    const size_t index = existing ? static_cast<size_t>(existing - records_) : count_++;
    const TrustedAccessPoint old = records_[index];
    auto& record = records_[index];
    record = TrustedAccessPoint{};
    std::memcpy(record.bssid, observed.bssid, 6);
    std::memcpy(record.ssid, observed.ssid, 33);
    record.auth_mode = observed.auth_mode;
    record.expected_channel = observed.channel;
    if (existing) std::memcpy(record.friendly_name, old.friendly_name, 33);
    if (save()) return true;
    records_[index] = old;
    if (!existing) --count_;
    return false;
}

bool TrustedApStore::untrust(const uint8_t* bssid) {
    if (!ready_) return false;
    if (!validBssid(bssid)) { error_ = Error::Invalid; return false; }
    const auto* record = find(bssid);
    if (!record) { error_ = Error::None; return true; }
    const size_t index = static_cast<size_t>(record - records_);
    const TrustedAccessPoint removed = *record;
    for (size_t i = index; i + 1 < count_; ++i) records_[i] = records_[i + 1];
    --count_;
    if (save()) return true;
    for (size_t i = count_; i > index; --i) records_[i] = records_[i - 1];
    records_[index] = removed;
    ++count_;
    return false;
}

bool TrustedApStore::configure(const uint8_t* bssid, const char* name, bool enabled) {
    if (!ready_) return false;
    const auto* found = find(bssid);
    if (!found || !name || std::strlen(name) > 32) { error_ = Error::Invalid; return false; }
    auto& record = records_[found - records_];
    const auto old = record;
    std::memset(record.friendly_name, 0, 33);
    std::strcpy(record.friendly_name, name);
    record.enabled = enabled;
    if (save()) return true;
    record = old;
    return false;
}

const char* TrustedApStore::errorText() const {
    switch (error_) {
        case Error::None: return "OK";
        case Error::Unavailable: return "Storage unavailable";
        case Error::Corrupt: return "Baseline corrupt";
        case Error::Incompatible: return "Unsupported baseline version";
        case Error::Full: return "Baseline full (32 APs)";
        case Error::Invalid: return "Invalid AP/configuration";
        case Error::SaveFailed: return "Save failed; trust unchanged";
    }
    return "Baseline error";
}
