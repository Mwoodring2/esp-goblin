#include "air_monitor.h"

namespace {
void count(AirCounters& counters, const WifiFrameEvent& event) {
    ++counters.management;
    if (event.channel >= 1 && event.channel <= 14) ++counters.channel[event.channel];
    switch (event.subtype) {
        case 8: ++counters.beacons; break;
        case 4: ++counters.probe_requests; break;
        case 5: ++counters.probe_responses; break;
        case 11: ++counters.authentication; break;
        case 12: ++counters.deauthentication; break;
        case 10: ++counters.disassociation; break;
        default: ++counters.other; break;
    }
}
void add(AirCounters& sum, const AirCounters& value) {
    sum.management += value.management; sum.beacons += value.beacons;
    sum.probe_requests += value.probe_requests; sum.probe_responses += value.probe_responses;
    sum.authentication += value.authentication; sum.deauthentication += value.deauthentication;
    sum.disassociation += value.disassociation; sum.other += value.other;
    sum.queue_drops += value.queue_drops;
    for (int ch = 1; ch <= 14; ++ch) sum.channel[ch] += value.channel[ch];
}
}
AirCounters& AirMonitor::bucket(uint64_t timestamp_us) {
    const uint64_t tick = timestamp_us / 100000;
    auto& bin = buckets_[tick % 10];
    if (bin.tick != tick) { bin.counts = AirCounters{}; bin.tick = tick; }
    return bin.counts;
}
void AirMonitor::advance(uint64_t now_us, uint32_t dropped) {
    if (now_us > latest_us_) latest_us_ = now_us;
    const uint32_t delta = dropped - last_dropped_;
    lifetime_.queue_drops += delta;
    bucket(now_us).queue_drops += delta;
    last_dropped_ = dropped;
}
AirCounters AirMonitor::rolling(uint64_t now_us) const {
    AirCounters result;
    const uint64_t tick = now_us / 100000;
    for (const auto& bin : buckets_)
        if (bin.tick <= tick && tick - bin.tick < 10) add(result, bin.counts);
    return result;
}
bool AirMonitor::process(const WifiFrameEvent& event, const TrustedApStore& baseline,
                         GuardAnalyzer& alerts, DisconnectBurst& burst) {
    count(lifetime_, event);
    if (event.timestamp_us > latest_us_) latest_us_ = event.timestamp_us;
    if (latest_us_ / 100000 - event.timestamp_us / 100000 < 10)
        count(bucket(event.timestamp_us), event);
    if (!detector_.observe(event, burst)) return false;
    alerts.recordDisconnectBurst(burst, baseline);
    if (burst.new_alert) ++notifications_;
    return burst.new_alert;
}
