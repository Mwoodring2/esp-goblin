// Host: c++ -std=c++11 -Iinclude src/access_point_inventory.cpp
//       src/trusted_ap_store.cpp src/guard_analyzer.cpp tests/guard_baseline_test.cpp -o guard_test
#include "guard_analyzer.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
static int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
class MemoryStorage : public TrustedApStorage {
public:
    std::vector<uint8_t> bytes;
    bool fail_read = false;
    bool fail_write = false;
    size_t writes = 0;
    ReadResult read(uint8_t* data, size_t capacity, size_t& size) override {
        if (fail_read || bytes.size() > capacity) return ReadResult::Error;
        if (bytes.empty()) return ReadResult::Missing;
        size = bytes.size(); std::memcpy(data, bytes.data(), size); return ReadResult::Ok;
    }
    bool write(const uint8_t* data, size_t size) override {
        ++writes;
        if (fail_write) return false;
        bytes.assign(data, data + size); return true;
    }
};
AccessPointRecord ap(int id, const char* ssid = "HomeWiFi", int auth = 7, int channel = 6) {
    AccessPointRecord record;
    record.bssid[0] = 2; record.bssid[5] = static_cast<uint8_t>(id);
    std::strncpy(record.ssid, ssid, 32);
    record.auth_mode = auth; record.channel = channel; record.rssi = -51;
    record.first_seen = 123456; record.last_seen = 999999; record.seen_count = 15;
    return record;
}
bool has(const GuardAnalyzer& analyzer, GuardAlertType type) {
    for (size_t i = 0; i < analyzer.count(); ++i) if (analyzer.at(i)->type == type) return true;
    return false;
}
void updateCrc(std::vector<uint8_t>& data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < data.size(); ++i) {
        if (i >= 8 && i < 12) continue;
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    crc = ~crc;
    for (int i = 0; i < 4; ++i) data[8 + i] = static_cast<uint8_t>(crc >> (8 * i));
}
int main() {
    MemoryStorage storage;
    TrustedApStore store(storage);
    check(store.load() && store.ready() && store.count() == 0, "missing baseline boots empty");
    auto home = ap(1);
    check(store.trust(home) && store.count() == 1 && store.isTrustedAccessPoint(home.bssid), "trust AP");
    check(storage.bytes.size() == 88, "wire format contains baseline only, no runtime timestamps");
    check(store.find(home.bssid) && !store.find(nullptr), "BSSID lookup");
    home.auth_mode = 0;
    const size_t writes = storage.writes;
    check(store.trust(home) && storage.writes == writes && store.find(home.bssid)->auth_mode == 7,
          "duplicate trust never accepts a changed security baseline");
    home.auth_mode = 7;
    check(store.configure(home.bssid, "Living room", false) && !store.isTrustedAccessPoint(home.bssid), "disable baseline");
    TrustedApStore reboot(storage);
    check(reboot.load() && reboot.count() == 1 && !reboot.isTrustedAccessPoint(home.bssid) &&
          !std::strcmp(reboot.find(home.bssid)->friendly_name, "Living room"), "restore disabled/name configuration");
    check(store.trust(home) && store.isTrustedAccessPoint(home.bssid), "explicit trust re-enables baseline");
    check(reboot.load() && reboot.isTrustedAccessPoint(home.bssid) && reboot.find(home.bssid)->expected_channel == 6 &&
          reboot.find(home.bssid)->auth_mode == 7, "restore baseline after reboot");
    AccessPointInventory inventory;
    inventory.beginScan(); inventory.observe(home.bssid, home.ssid, 6, -51, 7, 1);
    inventory.setKnown(home.bssid, reboot.isTrustedAccessPoint(home.bssid));
    check(inventory.knownCount() == 1, "baseline restores runtime known state");

    GuardAnalyzer analyzer;
    analyzer.observe(home, store, false);
    check(analyzer.count() == 0, "unchanged trusted AP has no alert");
    auto unknown = ap(2);
    analyzer.observe(unknown, store, true);
    check(has(analyzer, GuardAlertType::SsidCollision) && has(analyzer, GuardAlertType::NewAccessPoint), "exact SSID collision on unknown BSSID");
    const auto* collision = analyzer.at(0);
    check(collision && !std::memcmp(collision->wifi.trusted_bssid, home.bssid, 6) &&
          !std::memcmp(collision->wifi.bssid, unknown.bssid, 6) && collision->wifi.rssi == -51 && collision->wifi.channel == 6,
          "collision contains both BSSIDs and radio context");
    analyzer.observe(unknown, store, true);
    check(analyzer.count() == 2, "deduplicate events in one scan");
    analyzer.dismiss(0);
    check(analyzer.activeCount() == 1 && analyzer.firstImportant() == -1, "dismiss important alert");
    analyzer.beginScan(); unknown = ap(2, "Other"); analyzer.observe(unknown, store, false);
    check(analyzer.count() == 0, "no collision for different SSID");
    unknown = ap(2, "homewifi"); analyzer.observe(unknown, store, false);
    check(analyzer.count() == 0, "SSID comparison case sensitive");
    unknown = ap(2, "HomeWiFi "); analyzer.observe(unknown, store, false);
    check(analyzer.count() == 0, "SSID comparison does not trim whitespace");
    auto changed = ap(1, "Renamed", 0, 11);
    analyzer.observe(changed, store, false);
    check(has(analyzer, GuardAlertType::TrustedSsidChanged), "known BSSID SSID change");
    check(has(analyzer, GuardAlertType::TrustedSecurityChanged), "security downgrade change");
    check(has(analyzer, GuardAlertType::TrustedChannelChanged), "channel change");
    check(!has(analyzer, GuardAlertType::SsidCollision), "known BSSID not a collision");
    check(analyzer.firstImportant() >= 0 && analyzer.at(analyzer.firstImportant())->type == GuardAlertType::TrustedSecurityChanged,
          "security change prioritized");
    for (size_t i = 0; i < analyzer.count(); ++i) {
        const auto* event = analyzer.at(i);
        if (event->type == GuardAlertType::TrustedSecurityChanged)
            check(std::strstr(event->wifi.old_value, "WPA2/WPA3") && std::strstr(event->wifi.new_value, "OPEN"), "security old/new values");
    }
    check(!guardImportant(GuardAlertType::TrustedChannelChanged), "channel change informational");
    analyzer.beginScan(); unknown = ap(2); check(store.trust(unknown), "trust second BSSID same SSID");
    analyzer.observe(unknown, store, false);
    check(analyzer.count() == 0, "two trusted BSSIDs same SSID do not collide");
    check(store.configure(home.bssid, "Living room", false) && store.configure(unknown.bssid, "", false), "disable matching candidates");
    analyzer.observe(ap(3), store, false);
    check(analyzer.count() == 0, "disabled baselines do not produce collisions");
    check(store.trust(ap(4, "")), "trust hidden AP by BSSID");
    analyzer.observe(ap(5, ""), store, false);
    check(analyzer.count() == 0, "empty hidden SSIDs do not imply collision");

    storage.fail_write = true;
    const size_t before = store.count();
    check(!store.trust(ap(8)) && store.count() == before && !store.find(ap(8).bssid), "failed trust rolls back");
    check(!store.untrust(home.bssid) && store.find(home.bssid) && store.count() == before, "failed removal rolls back");
    check(!store.configure(home.bssid, "Changed", true) && !store.isTrustedAccessPoint(home.bssid) &&
          !std::strcmp(store.find(home.bssid)->friendly_name, "Living room"), "failed config rolls back");
    storage.fail_write = false;
    check(store.untrust(home.bssid) && !store.find(home.bssid), "untrust removes BSSID");
    check(reboot.load() && !reboot.find(home.bssid), "untrust persists across reboot");
    check(store.untrust(home.bssid), "duplicate untrust harmless");
    check(!store.untrust(nullptr), "null BSSID rejected");
    auto invalid = ap(9); invalid.bssid[0] = 1;
    check(!store.trust(invalid), "multicast BSSID rejected");
    invalid = ap(9); invalid.channel = 0;
    check(!store.trust(invalid), "invalid channel rejected");
    invalid = ap(9); std::memset(invalid.ssid, 'x', 33);
    check(!store.trust(invalid), "unterminated SSID rejected");

    MemoryStorage fullStorage; TrustedApStore full(fullStorage); full.load();
    for (size_t i = 0; i < TrustedApStore::Capacity; ++i) check(full.trust(ap(i + 1)), "fill baseline");
    check(!full.trust(ap(100)) && full.error() == TrustedApStore::Error::Full && full.count() == 32, "capacity check");
    check(full.trust(ap(1)), "duplicate trust succeeds at capacity");
    check(full.untrust(ap(1).bssid) && full.trust(ap(100)), "reuse removed capacity");
    const auto valid = fullStorage.bytes;
    fullStorage.bytes[20] ^= 0x40;
    check(!full.load() && !full.ready() && full.count() == 0 && !full.trust(home), "checksum corruption fails closed");
    fullStorage.bytes = valid; fullStorage.bytes[4] = 2;
    check(!full.load() && full.error() == TrustedApStore::Error::Incompatible, "unsupported schema rejected");
    fullStorage.bytes = valid; fullStorage.bytes.pop_back();
    check(!full.load(), "truncated data rejected");
    fullStorage.bytes = valid; std::memcpy(fullStorage.bytes.data() + 12 + 76, fullStorage.bytes.data() + 12, 6); updateCrc(fullStorage.bytes);
    check(!full.load() && full.count() == 0, "duplicate persisted BSSID rejected");
    fullStorage.bytes = valid; std::memset(fullStorage.bytes.data() + 18, 'x', 33); updateCrc(fullStorage.bytes);
    check(!full.load(), "unterminated persisted SSID rejected");
    fullStorage.bytes = valid; fullStorage.bytes[12 + 75] = 2; updateCrc(fullStorage.bytes);
    check(!full.load(), "invalid persisted enabled flag rejected");
    fullStorage.bytes = valid; fullStorage.fail_read = true;
    check(!full.load() && full.error() == TrustedApStore::Error::Unavailable, "read error safe");
    fullStorage.fail_read = false; check(full.load(), "valid storage can reload after error");

    analyzer.beginScan();
    for (int i = 40; i < 80; ++i) analyzer.observe(ap(i, "Other"), full, true);
    check(analyzer.count() == GuardAnalyzer::Capacity && analyzer.droppedCount() == 8, "bounded alert overflow");
    analyzer.observe(ap(100, "HomeWiFi", 0), full, false);
    check(has(analyzer, GuardAlertType::TrustedSecurityChanged) && analyzer.firstImportant() >= 0,
          "security change displaces informational event at capacity");
    std::printf("%s: %d baseline/analyzer checks (%d failures)\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
