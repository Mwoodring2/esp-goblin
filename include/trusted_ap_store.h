#pragma once
#include "access_point_inventory.h"

struct TrustedAccessPoint {
    uint8_t bssid[6] = {};
    char ssid[33] = {};
    int auth_mode = 0;
    int expected_channel = 0;
    char friendly_name[33] = {};
    bool enabled = true;
};

// Hardware-independent storage contract. Writes replace one complete blob.
class TrustedApStorage {
public:
    enum class ReadResult { Ok, Missing, Error };
    virtual ~TrustedApStorage() = default;
    virtual ReadResult read(uint8_t* data, size_t capacity, size_t& size) = 0;
    virtual bool write(const uint8_t* data, size_t size) = 0;
};

class TrustedApStore {
public:
    static constexpr size_t Capacity = 32;
    static constexpr size_t BlobCapacity = 12 + Capacity * 76;
    enum class Error { None, Unavailable, Corrupt, Incompatible, Full, Invalid, SaveFailed };
    explicit TrustedApStore(TrustedApStorage& storage) : storage_(storage) {}
    bool load();
    bool trust(const AccessPointRecord& observed);
    bool untrust(const uint8_t* bssid);
    bool configure(const uint8_t* bssid, const char* friendly_name, bool enabled);
    const TrustedAccessPoint* find(const uint8_t* bssid) const;
    const TrustedAccessPoint* at(size_t index) const { return index < count_ ? &records_[index] : nullptr; }
    bool isTrustedAccessPoint(const uint8_t* bssid) const;
    size_t count() const { return count_; }
    bool ready() const { return ready_; }
    Error error() const { return error_; }
    const char* errorText() const;
private:
    bool save();
    TrustedApStorage& storage_;
    TrustedAccessPoint records_[Capacity];
    uint8_t blob_[BlobCapacity] = {};
    size_t count_ = 0;
    bool ready_ = false;
    Error error_ = Error::Unavailable;
};
