#pragma once
#include "guard_analyzer.h"

struct AirCounters {
    uint64_t management = 0, beacons = 0, probe_requests = 0, probe_responses = 0;
    uint64_t authentication = 0, deauthentication = 0, disassociation = 0, other = 0;
    uint64_t queue_drops = 0;
    uint64_t channel[15] = {};
};

class AirMonitor {
public:
    bool process(const WifiFrameEvent& event, const TrustedApStore& baseline, GuardAnalyzer& alerts, DisconnectBurst& burst);
    void advance(uint64_t now_us, uint32_t dropped);
    const AirCounters& lifetime() const { return lifetime_; }
    AirCounters rolling(uint64_t now_us) const;
    uint32_t notifications() const { return notifications_; }
    uint32_t detectorEvictions() const { return detector_.evictions(); }
private:
    struct Bucket { uint64_t tick = UINT64_MAX; AirCounters counts; };
    AirCounters& bucket(uint64_t timestamp_us);
    Bucket buckets_[10]; // 100ms bins, approximately the trailing second.
    AirCounters lifetime_;
    DeauthDetector detector_;
    uint32_t last_dropped_ = 0, notifications_ = 0;
    uint64_t latest_us_ = 0;
};
