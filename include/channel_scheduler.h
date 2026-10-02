#pragma once
#include "air_config.h"

class ChannelScheduler {
public:
    bool configure(uint8_t first, uint8_t count, uint32_t now_ms) {
        if (first < 1 || !count || static_cast<unsigned>(first) + count - 1 > 14) return false;
        first_ = first; count_ = count; current_ = first; last_ms_ = now_ms;
        return true;
    }
    bool due(uint32_t now_ms) const { return count_ && now_ms - last_ms_ >= kChannelDwellMs; }
    uint8_t next() const { return current_ + 1 < first_ + count_ ? current_ + 1 : first_; }
    void advanced(uint32_t now_ms) { current_ = next(); last_ms_ = now_ms; }
    uint8_t channel() const { return current_; }
    uint8_t first() const { return first_; }
    uint8_t count() const { return count_; }
private:
    uint8_t first_ = 0, count_ = 0, current_ = 0;
    uint32_t last_ms_ = 0;
};

class WifiMonitorControl {
public:
    virtual ~WifiMonitorControl() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool running() const = 0;
};

// Restoration is attempted even on an early return / failed scan. A failed stop
// forbids the scan rather than allowing scanning and hopping to compete.
class WifiScanPause {
public:
    explicit WifiScanPause(WifiMonitorControl& monitor) : monitor_(monitor), resume_(monitor.running()) {
        if (resume_) monitor_.stop();
        safe_ = !monitor_.running();
    }
    ~WifiScanPause() { restore(); }
    WifiScanPause(const WifiScanPause&) = delete;
    WifiScanPause& operator=(const WifiScanPause&) = delete;
    bool safeToScan() const { return safe_; }
    bool restore() {
        if (!restored_) { restored_ = true; result_ = !resume_ || monitor_.start(); }
        return result_;
    }
private:
    WifiMonitorControl& monitor_;
    bool resume_, safe_ = false, restored_ = false, result_ = false;
};
