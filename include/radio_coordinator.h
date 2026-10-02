#pragma once
#include <cstdint>
constexpr uint32_t kWifiMonitorWindowMs = 5000, kBlePrivacyWindowMs = 2500, kRadioRetryMs = 1000;
enum class RadioMode { Idle, WifiMonitor, BlePrivacyScan, WifiApScan };
const char* radioModeName(RadioMode mode);
class RadioBackend {
public:
    virtual ~RadioBackend() = default;
    virtual bool wifiStart() = 0;
    virtual bool wifiStop() = 0;
    virtual bool wifiRunning() const = 0;
    virtual bool bleStart(uint32_t duration, uint32_t window) = 0;
    virtual bool bleInitializing() const { return false; }
    virtual bool bleStop() = 0;
    virtual bool bleRunning() const = 0;
    virtual bool bleReset() = 0;
};
class RadioCoordinator {
public:
    explicit RadioCoordinator(RadioBackend& backend) : backend_(backend) {}
    bool start(uint64_t now);
    bool stop(uint64_t now);
    void tick(uint64_t now);
    bool beginApScan(uint64_t now);
    bool endApScan(uint64_t now);
    RadioMode mode() const { return mode_; }
    bool enabled() const { return enabled_; }
    uint64_t wifiUptime() const { return wifi_uptime_; }
    uint64_t bleUptime() const { return ble_uptime_; }
    uint32_t failures() const { return failures_; }
    uint32_t nextBleMs(uint64_t now) const;
private:
    void account(uint64_t now);
    bool quietBle();
    bool wifi(uint64_t now);
    bool ble(uint64_t now);
    RadioBackend& backend_;
    RadioMode mode_ = RadioMode::Idle, before_scan_ = RadioMode::Idle;
    bool enabled_ = false, recovery_ = false;
    uint64_t since_ = 0, accounted_ = 0, retry_ = 0, wifi_uptime_ = 0, ble_uptime_ = 0;
    uint32_t failures_ = 0, window_ = 0;
};
