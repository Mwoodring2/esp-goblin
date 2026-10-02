#include "ble_privacy.h"
#include "radio_coordinator.h"
#include "guard_analyzer.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) { ++checks; if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); } }
std::vector<uint8_t> serviceFrame(uint8_t type, size_t length) {
    std::vector<uint8_t> data(length + 4); data[0] = length + 3; data[1] = 0x16; data[2] = 0xaa; data[3] = 0xfe; if (length) data[4] = type; return data;
}
BleParsedAdvertisement parse(const std::vector<uint8_t>& p) { return parseBleAdvertisement(p.data(), p.size()); }
BleObservationEvent observation(uint8_t id, uint64_t now, uint32_t window = 1) { BleObservationEvent e; e.key.address[5] = id; e.timestamp_ms = now; e.window = window; e.rssi = -60; return e; }
class Backend : public RadioBackend {
public:
    bool w = false, b = false, fail_start = false, fail_stop = false, fail_reset = false, fail_wifi = false, fail_wifi_stop = false;
    bool overlap = false; unsigned starts = 0, resets = 0; uint32_t duration = 0, window = 0;
    bool initializing = false;
    bool wifiStart() override { overlap |= b; w = !fail_wifi; return w; }
    bool wifiStop() override { if (!fail_wifi_stop) w = false; return !w; }
    bool wifiRunning() const override { return w; }
    bool bleStart(uint32_t d, uint32_t id) override { overlap |= w; ++starts; duration = d; window = id; b = true; return !fail_start; }
    bool bleStop() override { if (!fail_stop) b = false; return !b; }
    bool bleRunning() const override { return b; }
    bool bleInitializing() const override { return initializing; }
    bool bleReset() override { ++resets; if (!fail_reset) b = false; return !b; }
};
void parserTests() {
    check(parse({}).classification == BleDeviceClass::Unknown, "empty payload");
    check(parse({0, 255}).classification == BleDeviceClass::Unknown, "zero length terminates");
    check(parseBleAdvertisement(nullptr, 2).malformed, "null checked");
    check(parse({3, 0xff, 0x4c}).malformed, "truncated structure");
    check(parse({2, 0xff, 0x4c}).malformed, "short company field");
    auto p = parse({2, 1, 6, 3, 3, 0xaa, 0xfe, 4, 9, 'T', 'a', 'g', 3, 0xff, 0x4c, 0, 2, 0x0a, 0xc0});
    check(p.has_flags && p.flags == 6 && p.has_tx_power && p.tx_power == -64, "flags and signed TX power");
    check(p.service_count == 1 && p.services[0].length == 2 && p.services[0].bytes[0] == 0xaa, "16 bit UUID");
    check(!std::strcmp(p.name, "Tag") && p.complete_name, "complete name");
    check(p.has_company && p.company == 0x004c && p.classification == BleDeviceClass::GenericBle, "Apple is manufacturer only");
    check(p.confidence == DetectionConfidence::Low && !std::strcmp(bleCompanyName(p.company), "Apple") && !bleCompanyName(0xffff), "company subset and low classification confidence");
    p = parse({4, 9, 'F', 'u', 'l', 2, 8, 'S', 1, 0x99});
    check(!p.malformed && !std::strcmp(p.name, "Ful"), "complete name wins shortened duplicate; unknown ignored");
    p = parse({2, 3, 0xaa, 2, 9, 'X'});
    check(p.malformed && !p.name[0], "malformed UUID stops parsing");
    std::vector<uint8_t> multi = {17, 7}; multi.resize(18, 1);
    multi.insert(multi.end(), {5, 0x20, 1, 2, 3, 4, 17, 0x21}); multi.resize(multi.size() + 16, 2);
    p = parse(multi); check(!p.malformed && p.service_count == 3 && p.services[0].length == 16 && p.services[1].length == 4, "128-bit UUID and 32/128-bit service data");
    check(parse({1, 0x16}).malformed && parse({2, 0x20, 1}).malformed && parse({2, 0x21, 1}).malformed, "service headers bounded");
    p = parse({11, 3, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0}); check(p.service_count == 4 && p.service_overflow, "bounded service list");
    for (uint8_t type : {0x40, 0x41}) for (size_t n = 0; n < 40; ++n) {
        p = parse(serviceFrame(type, n));
        const bool valid = n == 22 || n == 34 || (type == 0x40 && (n == 21 || n == 33));
        check((p.classification == BleDeviceClass::GoogleFindHub) == valid, "Find Hub exact length matrix");
        if (valid) check(p.confidence == DetectionConfidence::High, "Find Hub protocol confidence");
    }
    check(parse(serviceFrame(0x42, 22)).classification == BleDeviceClass::GenericBle, "unknown FEAA is not Find Hub");
    for (auto spec : {std::pair<uint8_t, size_t>(0, 20), {0x10, 4}, {0x20, 14}, {0x30, 10}}) {
        auto data = serviceFrame(spec.first, spec.second); p = parse(data);
        check(p.classification == BleDeviceClass::Eddystone && p.confidence == DetectionConfidence::High, "four Eddystone types");
        data.pop_back(); check(parse(data).classification != BleDeviceClass::Eddystone, "truncated Eddystone rejected");
    }
    auto encrypted = serviceFrame(0x20, 18); encrypted[5] = 1;
    check(parse(encrypted).classification == BleDeviceClass::Eddystone, "encrypted telemetry format only");
    auto badurl = serviceFrame(0x10, 4); badurl[6] = 4;
    check(parse(badurl).classification != BleDeviceClass::Eddystone, "invalid URL scheme rejected");
    auto combined = serviceFrame(0x40, 22); auto beacon = serviceFrame(0x30, 10); combined.insert(combined.end(), beacon.begin(), beacon.end());
    check(parse(combined).classification == BleDeviceClass::GoogleFindHub, "duplicate service fields retain strongest rule");
    size_t count = 0; const auto* registry = bleSignatures(count);
    check(count == 5 && registry[0].source && registry[0].license && registry[0].matches, "signature provenance registry");
    // Deterministic malformed-input sweep; sanitizer-compatible, no hardware needed.
    uint32_t random = 7;
    for (size_t n = 0; n <= 64; ++n) for (unsigned j = 0; j < 100; ++j) {
        std::vector<uint8_t> bytes(n); for (auto& value : bytes) { random = random * 1664525 + 1013904223; value = random >> 24; }
        const auto parsed = parse(bytes); check(parsed.service_count <= 4 && parsed.name[32] == 0, "bounded random AD parse");
    }
}
void inventoryTests() {
    BleDeviceInventory inventory;
    auto parsed = parse({2, 9, 'A', 3, 0xff, 0x4c, 0}); auto e = observation(1, 100);
    auto* r = inventory.observe(e, parsed); check(r && r->first_seen == 100 && r->seen_count == 1, "inventory insert");
    e.timestamp_ms = 200; e.rssi = -40; r = inventory.observe(e, parse({2, 1, 6}));
    check(r->last_seen == 200 && r->seen_count == 2 && r->strongest_rssi == -40 && r->advertisement.name[0] == 'A', "repeat updates and keeps optional metadata");
    e.timestamp_ms = 300; e.rssi = -70; r = inventory.observe(e, parsed); check(r->rssi == -70 && r->strongest_rssi == -40, "strongest retained");
    e.key.type = 1; r = inventory.observe(e, parsed); check(inventory.count() == 2 && r->stability == BleIdentityStability::RandomAddress, "address type is key; random not stable");
    e.key.address[5] = 2; inventory.observe(e, parsed); check(inventory.count() == 3, "similar manufacturer separate identity");
    BleDeviceInventory full;
    for (size_t i = 0; i < kBleInventoryCapacity; ++i) full.observe(observation(i, 100 + i), parsed);
    full.setPersistent(observation(0, 0).key, true);
    full.observe(observation(100, 1000), parsed);
    check(full.count() == kBleInventoryCapacity && full.evictions() == 1 && full.find(observation(0, 0).key) && !full.find(observation(1, 0).key), "oldest nonpersistent eviction");
    for (size_t i = 0; i < full.count(); ++i) full.setPersistent(full.at(i)->key, true);
    check(!full.observe(observation(101, 2000), parsed) && full.drops() == 1, "all protected drops new record");
    check(!std::strcmp(bleSignalCategory(-86), "VERY WEAK") && !std::strcmp(bleSignalCategory(-85), "WEAK"), "RSSI -85 boundary");
    check(!std::strcmp(bleSignalCategory(-76), "WEAK") && !std::strcmp(bleSignalCategory(-75), "MEDIUM"), "RSSI -75 boundary");
    check(!std::strcmp(bleSignalCategory(-66), "MEDIUM") && !std::strcmp(bleSignalCategory(-65), "STRONG"), "RSSI -65 boundary");
    check(!std::strcmp(bleSignalCategory(-51), "STRONG") && !std::strcmp(bleSignalCategory(-50), "VERY STRONG"), "RSSI -50 boundary");
}
void privacyTests() {
    PrivacyWatchAnalyzer watch;
    const auto cls = BleDeviceClass::GoogleFindHub;
    auto e = observation(1, 0); auto* c = watch.observe(e, cls);
    check(c && !c->persistent && c->windows == 1, "nearby initially not persistent");
    e.timestamp_ms = 50000; c = watch.observe(e, cls); check(c->windows == 1, "duplicate advertisements not windows");
    for (unsigned i = 1; i <= 5; ++i) { e.timestamp_ms = i * 100000; e.window = i + 1; c = watch.observe(e, cls); }
    check(c && !c->persistent, "below ten minutes");
    e.timestamp_ms = 600000; e.window = 7; c = watch.observe(e, cls); check(c->persistent, "ten minute threshold with distinct windows");
    GuardAnalyzer alerts; BleDeviceInventory inventory;
    auto parsed = parse(serviceFrame(0x40, 22)); auto* record = inventory.observe(e, parsed);
    PrivacyCandidate nearby = *c; nearby.persistent = false;
    check(alerts.recordBle(*record, nearby) && alerts.at(0)->type == GuardAlertType::BleTrackerCapableDevice, "BLE nearby alert envelope");
    check(alerts.at(0)->wifi.bssid[5] == 0 && alerts.at(0)->ble.key.address[5] == 1, "BLE identity never in WiFi fields");
    alerts.dismiss(0); check(!alerts.recordBle(*record, nearby) && alerts.at(0)->dismissed, "dismiss survives repeat");
    check(alerts.recordBle(*record, *c) && !alerts.at(0)->dismissed, "persistent escalation re-arms alert");
    alerts.beginScan(); check(alerts.count() == 1 && alerts.firstImportant() == 0, "AP scan preserves BLE alert");
    watch.expire(719999); check(watch.find(e.key), "below expiry retained");
    watch.expire(720000); check(!watch.find(e.key), "expiry boundary");
    alerts.expireBle(720000); check(alerts.count() == 0, "stale alerts expire");
    e.timestamp_ms = 720000; c = watch.observe(e, cls); check(c && !c->persistent && c->windows == 1, "reappearance starts new span");
    e.key.address[5] = 2; c = watch.observe(e, cls); check(c && !c->persistent && watch.count() == 2, "rotated address separate");
    check(!watch.observe(observation(3, 720000), BleDeviceClass::Eddystone), "beacons not candidates");
    PrivacyWatchAnalyzer bounded;
    for (size_t i = 0; i < kPrivacyCandidateCapacity + 1; ++i) bounded.observe(observation(i, i), cls);
    check(bounded.count() == kPrivacyCandidateCapacity && bounded.evictions() == 1 && !bounded.find(observation(0, 0).key), "bounded candidates oldest first");
    PrivacyWatchAnalyzer protectedCandidates;
    for (unsigned window = 0; window <= 6; ++window)
        for (unsigned id = 0; id < kPrivacyCandidateCapacity; ++id)
            protectedCandidates.observe(observation(id, window * 100000, window + 1), cls);
    check(!protectedCandidates.observe(observation(100, 600001, 8), cls) && protectedCandidates.drops() == 1,
        "all persistent candidates protected at capacity");
    GuardAnalyzer protectedAlerts;
    for (unsigned id = 0; id < 8; ++id) {
        record = inventory.observe(observation(id, 600000, 7), parsed);
        const auto* candidate = protectedCandidates.find(record->key);
        check(candidate && protectedAlerts.recordBle(*record, *candidate), "persistent alert insertion");
    }
    record = inventory.observe(observation(9, 600000, 7), parsed);
    check(!protectedAlerts.recordBle(*record, *protectedCandidates.find(record->key)) && protectedAlerts.count() == 8,
        "active persistent alerts not evicted for new BLE alert");
    PrivacyWatchAnalyzer fewWindows;
    for (unsigned i = 0; i <= 6; ++i) c = fewWindows.observe(observation(1, i * 100000, 1), cls);
    check(c && !c->persistent, "duration alone insufficient");
}
void radioTests() {
    Backend b; RadioCoordinator r(b);
    check(r.mode() == RadioMode::Idle && r.start(0) && b.w && !b.b, "start WiFi");
    r.tick(4999); check(b.w && r.nextBleMs(4999) == 1, "WiFi window held");
    r.tick(5000); check(!b.w && b.b && r.mode() == RadioMode::BlePrivacyScan && b.duration == 2500, "passive BLE window transition");
    r.tick(7500); check(b.w && !b.b && r.wifiUptime() == 5000 && r.bleUptime() == 2500, "restore and uptime");
    r.tick(12500); check(b.window == 2 && b.b, "next distinct scan window");
    check(r.beginApScan(13000) && !b.w && !b.b && r.mode() == RadioMode::WifiApScan, "AP scan stops both listeners");
    r.tick(15000); check(!b.w && !b.b, "coordinator paused during AP scan");
    check(r.endApScan(16000) && b.b && !b.w, "AP scan restores previous BLE mode");
    check(r.stop(17000) && !b.w && !b.b && !r.enabled(), "explicit stop both");
    check(r.beginApScan(18000) && r.endApScan(19000) && r.mode() == RadioMode::Idle, "idle AP scan stays idle");
    b.fail_start = true; r.start(20000); r.tick(25000);
    check(b.w && !b.b && r.failures() >= 1, "failed BLE start including dirty active state restores WiFi");
    b.fail_start = false; r.tick(30000); b.fail_stop = true; r.tick(32500);
    check(b.w && !b.b && b.resets == 1, "stop failure resets BLE then restores WiFi");
    b.fail_stop = false; r.tick(37500); b.fail_stop = true; b.fail_reset = true; r.tick(40000);
    check(!b.w && b.b, "failed reset does not enable both radios");
    b.fail_reset = false; r.tick(41000); check(b.w && !b.b, "recovery retries and returns WiFi");
    b.fail_stop = false; b.fail_wifi_stop = true; r.tick(46000); check(b.w && !b.b, "WiFi stop failure never starts BLE");
    b.fail_wifi_stop = false; r.tick(51000); b.b = false; r.tick(51100); check(b.w, "early scan end restores WiFi");
    check(!b.overlap, "all transitions exclusive");
    Backend failed; failed.fail_wifi = true; RadioCoordinator recover(failed);
    check(!recover.start(0), "WiFi start failure reported"); failed.fail_wifi = false; recover.tick(1000); check(failed.w, "WiFi start recovery retry");
    Backend pending; pending.fail_start = true; pending.initializing = true; RadioCoordinator async(pending);
    async.start(0); async.tick(5000);
    check(pending.w && !pending.b && async.failures() == 0, "pending initialization retains WiFi without false failure");
    Backend scan; RadioCoordinator ap(scan); ap.start(0);
    check(ap.beginApScan(50) && ap.endApScan(100) && scan.w, "failed or empty AP scan also restores previous WiFi mode");
    ap.tick(5100); scan.fail_stop = true; scan.fail_reset = true;
    check(!ap.beginApScan(5200) && !scan.w && scan.b, "AP scan refused when BLE cannot stop");
    scan.fail_reset = false; ap.tick(6200); check(scan.w && !scan.b, "aborted AP scan recovery restores WiFi");
    scan.fail_stop = false; ap.tick(11200); scan.fail_stop = true; scan.fail_reset = true;
    check(!ap.stop(11300), "stop reports failed cleanup");
    scan.fail_reset = false; ap.tick(12300); check(ap.mode() == RadioMode::Idle && !scan.b && !scan.w, "stop cleanup retries while disabled");
    scan.fail_stop = false; ap.start(13000); ap.tick(18000);
    check(ap.beginApScan(18100), "AP scan begins from BLE");
    scan.fail_start = true;
    check(!ap.endApScan(19000) && scan.w && !scan.b, "BLE restore failure reported while WiFi fallback succeeds");
}
void queueTests() {
    BleEventQueue q; auto e = observation(1, 0);
    for (size_t i = 0; i < kBleQueueCapacity; ++i) check(q.push(e), "queue fill");
    check(!q.push(e) && q.dropped() == 1, "queue bounded overflow");
    for (size_t i = 0; i < kBleQueueCapacity; ++i) check(q.pop(e), "queue drain");
    check(!q.pop(e), "queue empty");
    constexpr unsigned total = 20000; bool ordered = true;
    std::thread producer([&] { for (unsigned i = 0; i < total; ++i) { auto event = observation(1, i); while (!q.push(event)) std::this_thread::yield(); } });
    for (unsigned i = 0; i < total; ++i) { while (!q.pop(e)) std::this_thread::yield(); if (e.timestamp_ms != i) ordered = false; }
    producer.join(); check(ordered, "concurrent callback queue ordered and lossless on retry");
}
}
int main() {
    parserTests(); inventoryTests(); privacyTests(); radioTests(); queueTests();
    std::printf("BLE privacy: %d checks, %d failures\n", checks, failures); return failures ? 1 : 0;
}
