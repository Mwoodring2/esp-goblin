#include "radio_coordinator.h"
const char* radioModeName(RadioMode m) { switch (m) { case RadioMode::WifiMonitor: return "WifiMonitor"; case RadioMode::BlePrivacyScan: return "BlePrivacyScan"; case RadioMode::WifiApScan: return "WifiApScan"; default: return "Idle"; } }
void RadioCoordinator::account(uint64_t now) {
    if (now >= accounted_) { if (mode_ == RadioMode::WifiMonitor) wifi_uptime_ += now - accounted_; else if (mode_ == RadioMode::BlePrivacyScan) ble_uptime_ += now - accounted_; }
    accounted_ = now;
}
bool RadioCoordinator::quietBle() {
    if (backend_.bleStop() && !backend_.bleRunning()) return true;
    ++failures_;
    if (backend_.bleReset() && !backend_.bleRunning()) return true;
    ++failures_; return false;
}
bool RadioCoordinator::wifi(uint64_t now) {
    account(now); retry_ = now;
    if (!quietBle()) { mode_ = RadioMode::BlePrivacyScan; recovery_ = true; return false; }
    if (!backend_.wifiRunning() && !backend_.wifiStart()) { ++failures_; mode_ = RadioMode::Idle; recovery_ = true; return false; }
    mode_ = RadioMode::WifiMonitor; since_ = now; recovery_ = false; return true;
}
bool RadioCoordinator::ble(uint64_t now) {
    account(now);
    if (!backend_.wifiStop() || backend_.wifiRunning()) { ++failures_; since_ = now; return false; }
    mode_ = RadioMode::Idle;
    if (!backend_.bleStart(kBlePrivacyWindowMs, ++window_)) {
        if (!backend_.bleInitializing()) ++failures_;
        return wifi(now);
    }
    mode_ = RadioMode::BlePrivacyScan; since_ = now; return true;
}
bool RadioCoordinator::start(uint64_t now) { enabled_ = true; if (mode_ == RadioMode::WifiApScan) return false; return wifi(now); }
bool RadioCoordinator::stop(uint64_t now) {
    account(now); enabled_ = false; retry_ = now;
    const bool b = quietBle(); const bool w = backend_.wifiStop() && !backend_.wifiRunning();
    if (!w) ++failures_;
    mode_ = !b ? RadioMode::BlePrivacyScan : !w ? RadioMode::WifiMonitor : RadioMode::Idle;
    recovery_ = !b || !w; return !recovery_;
}
void RadioCoordinator::tick(uint64_t now) {
    account(now);
    if (mode_ == RadioMode::WifiApScan) return;
    if (recovery_) { if (now - retry_ >= kRadioRetryMs) { if (enabled_) wifi(now); else stop(now); } return; }
    if (!enabled_) return;
    if (mode_ == RadioMode::Idle) { wifi(now); return; }
    if (mode_ == RadioMode::WifiMonitor && (!backend_.wifiRunning() || now - since_ >= kWifiMonitorWindowMs)) { if (!backend_.wifiRunning()) wifi(now); else ble(now); }
    else if (mode_ == RadioMode::BlePrivacyScan && (!backend_.bleRunning() || now - since_ >= kBlePrivacyWindowMs)) wifi(now);
}
bool RadioCoordinator::beginApScan(uint64_t now) {
    account(now); if (mode_ == RadioMode::WifiApScan) return false;
    before_scan_ = mode_;
    if (!quietBle()) { recovery_ = true; retry_ = now; return false; }
    if (!backend_.wifiStop() || backend_.wifiRunning()) { ++failures_; if (enabled_) wifi(now); return false; }
    mode_ = RadioMode::WifiApScan; recovery_ = false; return true;
}
bool RadioCoordinator::endApScan(uint64_t now) {
    account(now); if (mode_ != RadioMode::WifiApScan) return !recovery_;
    mode_ = RadioMode::Idle;
    if (!enabled_) return true;
    if (before_scan_ == RadioMode::BlePrivacyScan) {
        ble(now);
        return mode_ == RadioMode::BlePrivacyScan && !recovery_;
    }
    return wifi(now);
}
uint32_t RadioCoordinator::nextBleMs(uint64_t now) const { return enabled_ && mode_ == RadioMode::WifiMonitor && now >= since_ && now - since_ < kWifiMonitorWindowMs ? kWifiMonitorWindowMs - static_cast<uint32_t>(now - since_) : 0; }
