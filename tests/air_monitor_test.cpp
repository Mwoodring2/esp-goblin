#include "air_monitor.h"
#include "wifi_frame_queue.h"
#include "channel_scheduler.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* text) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", text); }
}
std::vector<uint8_t> frame(uint8_t subtype, size_t body) {
    std::vector<uint8_t> bytes(24 + body);
    bytes[0] = subtype << 4;
    const uint8_t destination[6] = {0x10, 2, 3, 4, 5, 6};
    const uint8_t source[6] = {0x20, 7, 8, 9, 10, 11};
    const uint8_t bssid[6] = {0x30, 12, 13, 14, 15, 16};
    std::memcpy(bytes.data() + 4, destination, 6);
    std::memcpy(bytes.data() + 10, source, 6);
    std::memcpy(bytes.data() + 16, bssid, 6);
    return bytes;
}
bool parse(const std::vector<uint8_t>& data, WifiFrameEvent& out) {
    return parseWifiManagementFrame(data.data(), data.size(), 123456, -47, 6, out);
}
WifiFrameEvent disconnect(uint64_t time, uint8_t id = 1, uint8_t subtype = 12) {
    WifiFrameEvent event{};
    event.timestamp_us = time; event.bssid[0] = 2; event.bssid[5] = id;
    event.source[0] = 4; event.source[5] = 99; // source is deliberately NOT BSSID
    event.subtype = subtype; event.channel = 6; event.rssi = -45;
    event.has_reason_code = true; event.reason_code = 7;
    return event;
}
class Storage : public TrustedApStorage {
    ReadResult read(uint8_t*, size_t, size_t&) override { return ReadResult::Missing; }
    bool write(const uint8_t*, size_t) override { return true; }
};
class Monitor : public WifiMonitorControl {
public:
    bool active = false, fail_start = false, fail_stop = false;
    int starts = 0, stops = 0;
    bool start() override { ++starts; active = !fail_start; return active; }
    void stop() override { ++stops; if (!fail_stop) active = false; }
    bool running() const override { return active; }
};
}

int main() {
    WifiFrameEvent parsed{};
    const uint8_t subtypes[] = {0, 1, 2, 3, 4, 5, 8, 10, 11, 12, 13, 14};
    const size_t bodies[] = {4, 6, 10, 6, 0, 12, 12, 2, 6, 2, 1, 1};
    for (size_t i = 0; i < sizeof(subtypes); ++i) {
        auto bytes = frame(subtypes[i], bodies[i]);
        check(parse(bytes, parsed) && parsed.subtype == subtypes[i], "supported management subtype classification");
        check(parsed.timestamp_us == 123456 && parsed.rssi == -47 && parsed.channel == 6, "RX metadata preserved");
        if (bodies[i]) { bytes.pop_back(); check(!parse(bytes, parsed), "truncated fixed body rejected"); }
    }
    auto bytes = frame(12, 2); bytes[24] = 0x34; bytes[25] = 0x12;
    check(parse(bytes, parsed) && parsed.has_reason_code && parsed.reason_code == 0x1234, "little-endian reason code");
    check(!std::memcmp(parsed.destination, bytes.data() + 4, 6) &&
        !std::memcmp(parsed.source, bytes.data() + 10, 6) && !std::memcmp(parsed.bssid, bytes.data() + 16, 6), "all MAC addresses independently extracted");
    bytes[1] = 0x40;
    check(parse(bytes, parsed) && parsed.protected_frame && !parsed.has_reason_code && parsed.reason_code == 0, "protected body is not decoded as plaintext");
    check(!parseWifiManagementFrame(nullptr, 26, 1, -40, 1, parsed), "null frame rejected");
    for (size_t length = 0; length < 24; ++length) {
        auto short_frame = frame(12, 2); short_frame.resize(length);
        check(!parse(short_frame, parsed), "short header safely rejected");
    }
    bytes = frame(8, 12); bytes[0] |= 1;
    check(!parse(bytes, parsed), "invalid protocol version rejected");
    bytes = frame(8, 12); bytes[0] |= 8;
    check(!parse(bytes, parsed), "data frame rejected");
    bytes = frame(8, 12); bytes[0] |= 4;
    check(!parse(bytes, parsed), "control frame rejected");
    bytes = frame(8, 12); bytes[1] = 3;
    check(!parse(bytes, parsed), "management DS bits rejected");
    bytes = frame(8, 12); bytes[1] = 4;
    check(!parse(bytes, parsed), "more-fragments rejected");
    bytes = frame(8, 12); bytes[22] = 1;
    check(!parse(bytes, parsed), "nonzero fragment index rejected");
    bytes = frame(6, 10); check(!parse(bytes, parsed), "reserved subtype rejected");
    bytes = frame(12, 6); bytes[1] = 0x80; check(!parse(bytes, parsed), "unsupported ordered deauth header rejected");
    bytes = frame(13, 5); bytes[1] = 0x80; check(parse(bytes, parsed), "bounded action HT control header");
    bytes.pop_back(); check(!parse(bytes, parsed), "short HT control body rejected");
    bytes = frame(4, 0);
    check(!parseWifiManagementFrame(bytes.data(), bytes.size(), 0, -40, 0, parsed), "invalid RX channel rejected");

    WifiFrameQueue queue;
    check(queue.lockFree() && queue.queued() == 0 && !queue.pop(parsed), "empty lock-free queue");
    for (unsigned i = 0; i < WifiFrameQueue::Capacity; ++i) check(queue.push(disconnect(i)), "fill queue");
    check(!queue.push(disconnect(999)) && queue.dropped() == 1 && queue.highWater() == 128, "bounded overflow and high-water");
    for (unsigned i = 0; i < WifiFrameQueue::Capacity; ++i) check(queue.pop(parsed) && parsed.timestamp_us == i, "queue FIFO");
    for (unsigned i = 0; i < 1000; ++i) { queue.push(disconnect(i)); check(queue.pop(parsed) && parsed.timestamp_us == i, "ring wraps repeatedly"); }
    queue.reset(); check(queue.queued() == 0 && queue.dropped() == 0 && queue.highWater() == 0, "quiescent reset");
    constexpr unsigned ConcurrentFrames = 20000;
    std::thread producer([&queue]() {
        for (unsigned i = 0; i < ConcurrentFrames; ++i)
            while (!queue.push(disconnect(i))) std::this_thread::yield();
    });
    bool fifo = true;
    for (unsigned i = 0; i < ConcurrentFrames; ++i) {
        while (!queue.pop(parsed)) std::this_thread::yield();
        if (parsed.timestamp_us != i || parsed.reason_code != 7 || parsed.source[5] != 99) fifo = false;
    }
    producer.join();
    check(fifo && queue.queued() == 0, "concurrent SPSC preserves complete events and ordering");

    DeauthDetector detector;
    DisconnectBurst burst;
    for (unsigned i = 0; i < 9; ++i) check(!detector.observe(disconnect(i * 100000), burst), "burst below threshold");
    check(detector.observe(disconnect(900000), burst) && burst.frame_count == 10 && burst.new_alert, "burst exactly threshold");
    check(detector.observe(disconnect(1000000, 1, 10), burst) && burst.frame_count == 11 && !burst.new_alert,
          "above threshold combines disassociation without notification flood");
    check(burst.source[0] == 4 && burst.bssid[0] == 2 && burst.reason_code == 7, "source identity and reason retained separately");
    check(!detector.observe(disconnect(1100000, 2), burst), "different BSSID independent");
    check(!detector.observe(disconnect(3100001), burst), "burst expires after window");
    check(!detector.observe(disconnect(1), burst), "out-of-order event ignored");
    detector.reset();
    for (int i = 0; i < 9; ++i) detector.observe(disconnect(0), burst);
    check(detector.observe(disconnect(2000000), burst) && burst.frame_count == 10, "inclusive exact rolling window boundary");
    check(!detector.observe(disconnect(2000001), burst), "one microsecond beyond boundary expires old frames");
    detector.reset();
    for (int i = 0; i < 100; ++i) detector.observe(disconnect(i), burst);
    check(burst.frame_count == 64 && burst.count_lower_bound, "saturated history reported as lower bound");
    detector.reset();
    for (int i = 1; i <= 17; ++i) detector.observe(disconnect(i, i), burst);
    check(detector.evictions() == 1, "bounded BSSID tracking reports eviction");
    auto invalid = disconnect(100); invalid.bssid[0] = 0xff;
    check(!detector.observe(invalid, burst), "broadcast BSSID not attributed to AP");
    detector.reset();
    for (unsigned i = 0; i < 9; ++i) detector.observe(disconnect(i), burst);
    auto protected_event = disconnect(9); protected_event.has_reason_code = false;
    check(detector.observe(protected_event, burst) && !burst.has_reason_code, "protected disconnect participates without fabricated reason");

    ChannelScheduler channels;
    check(!channels.configure(0, 11, 0) && !channels.configure(1, 0, 0) && !channels.configure(10, 6, 0), "invalid country ranges rejected");
    check(channels.configure(6, 3, 0) && channels.channel() == 6 && !channels.due(299) && channels.due(300), "configured range and dwell");
    channels.advanced(300); check(channels.channel() == 7, "channel advance");
    channels.advanced(600); channels.advanced(900); check(channels.channel() == 6, "hop wraps inside configured range");
    channels.configure(1, 11, UINT32_MAX - 100);
    check(!channels.due(100) && channels.due(200), "millis rollover safe scheduler");
    Monitor monitor; monitor.active = true;
    { WifiScanPause pause(monitor); check(pause.safeToScan() && !monitor.running(), "scan pauses running monitor"); }
    check(monitor.running() && monitor.starts == 1 && monitor.stops == 1, "restore on scope exit including failed scan");
    monitor.active = false;
    { WifiScanPause pause(monitor); check(pause.safeToScan() && pause.restore(), "stopped monitor scan allowed"); }
    check(!monitor.running() && monitor.starts == 1, "stopped state preserved");
    monitor.active = true; monitor.fail_start = true;
    { WifiScanPause pause(monitor); check(!pause.restore(), "failed restore exposed"); }
    check(monitor.starts == 2, "restore attempted once");
    monitor.active = true; monitor.fail_stop = true; monitor.fail_start = false;
    { WifiScanPause pause(monitor); check(!pause.safeToScan(), "failed stop aborts scan"); }

    Storage storage; TrustedApStore baseline(storage); baseline.load();
    AccessPointRecord trusted;
    trusted.bssid[0] = 2; trusted.bssid[5] = 1;
    std::strcpy(trusted.ssid, "HomeWiFi"); trusted.channel = 6; trusted.auth_mode = 3;
    baseline.trust(trusted);
    GuardAnalyzer alerts; AirMonitor air;
    for (unsigned i = 0; i < 11; ++i) air.process(disconnect(i * 10000), baseline, alerts, burst);
    check(air.lifetime().management == 11 && air.lifetime().deauthentication == 11 && air.lifetime().channel[6] == 11, "lifetime and channel counters");
    check(air.rolling(110000).management == 11 && air.rolling(1200000).management == 0, "rolling counts expire");
    check(alerts.count() == 1 && alerts.at(0)->wifi.trusted_ap && alerts.at(0)->wifi.frame_count == 11 && alerts.at(0)->wifi.source[0] == 4,
          "trusted burst alert context without source inference");
    alerts.dismiss(0);
    air.process(disconnect(120000), baseline, alerts, burst);
    check(alerts.at(0)->dismissed && air.notifications() == 1, "dismiss survives continuous updates");
    alerts.beginScan(); check(alerts.count() == 1, "AP scan retains passive alert");
    for (unsigned i = 0; i < 10; ++i) air.process(disconnect(4000000 + i * 1000), baseline, alerts, burst);
    check(!alerts.at(0)->dismissed && air.notifications() == 2, "new episode re-arms notification");
    air.advance(4100000, 3);
    check(air.lifetime().queue_drops == 3 && air.rolling(4100000).queue_drops == 3, "drop counters");
    air.process(disconnect(1, 4), baseline, alerts, burst);
    check(air.rolling(4100000).queue_drops == 3, "delayed old event cannot erase current rolling bucket");
    for (auto subtype : {uint8_t(8), uint8_t(4), uint8_t(5), uint8_t(11), uint8_t(10), uint8_t(0)})
        air.process(disconnect(4200000, 9, subtype), baseline, alerts, burst);
    check(air.lifetime().beacons == 1 && air.lifetime().probe_requests == 1 && air.lifetime().probe_responses == 1 &&
        air.lifetime().authentication == 1 && air.lifetime().disassociation == 1 && air.lifetime().other == 1, "all counter categories");
    GuardAnalyzer crowded;
    for (int i = 1; i <= 12; ++i) {
        DisconnectBurst report;
        report.bssid[0] = 2; report.bssid[5] = i; report.timestamp_us = i; report.new_alert = true;
        crowded.recordDisconnectBurst(report, baseline);
    }
    check(crowded.count() == GuardAnalyzer::DisconnectCapacity, "passive history leaves capacity for Phase 2 alerts");
    auto collision = trusted; collision.bssid[5] = 88;
    crowded.observe(collision, baseline, false);
    check(crowded.count() == GuardAnalyzer::DisconnectCapacity + 1, "SSID collision still displayed with full passive history");
    std::printf("%s: %d air checks, %d failures; event=%zu bytes, queue capacity=%u\n",
        failures ? "FAIL" : "PASS", checks, failures, sizeof(WifiFrameEvent), WifiFrameQueue::Capacity);
    return failures ? 1 : 0;
}
