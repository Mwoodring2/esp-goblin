#pragma once
#include "ble_privacy.h"
#include "radio_coordinator.h"
RadioCoordinator& goblinRadio();
BleDeviceInventory& goblinBleInventory();
const PrivacyWatchAnalyzer& goblinPrivacy();
uint64_t goblinRadioNow();
void goblinBleTick();
uint32_t goblinBleNotifications();
uint32_t goblinBleQueueDrops();
uint64_t goblinBleObservations();
// Scope guard shared by both AP scan entry points, including early failures.
class GoblinApScanPause {
public:
    GoblinApScanPause() : safe_(goblinRadio().beginApScan(goblinRadioNow())) {}
    ~GoblinApScanPause() { restore(); }
    bool safeToScan() const { return safe_; }
    bool restore() { if (!restored_) { restored_ = true; result_ = safe_ && goblinRadio().endApScan(goblinRadioNow()); } return result_; }
private:
    bool safe_, restored_ = false, result_ = false;
};
