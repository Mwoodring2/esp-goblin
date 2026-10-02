#include "goblin_ble.h"
#include "goblin_events.h"
#include "goblin_guard.h"
#include "goblin_air.h"
#include <NimBLEDevice.h>
#include <esp_timer.h>
#include <Arduino.h>
#include <cstring>
#include <nvs_flash.h>

namespace {
BleEventQueue queue;
BleDeviceInventory inventory;
PrivacyWatchAnalyzer privacy;
std::atomic<uint32_t> scan_window{0}, truncated{0};
std::atomic<bool> accepting{false}, scan_ended{false};
std::atomic<int> scan_reason{0};
uint64_t observations = 0;
uint32_t malformed = 0, notifications = 0;
class Callbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (!device || !accepting.load(std::memory_order_acquire)) return;
        BleObservationEvent event;
        event.timestamp_ms = static_cast<uint64_t>(esp_timer_get_time()) / 1000;
        event.window = scan_window.load(); event.rssi = device->getRSSI();
        const auto* address = device->getAddress().getBase();
        if (!address) return;
        // Store display/network order, not NimBLE's little-endian address bytes.
        for (unsigned i = 0; i < 6; ++i) event.key.address[i] = address->val[5 - i];
        event.key.type = device->getAddressType();
        const auto& payload = device->getPayload();
        event.payload_length = payload.size() > sizeof(event.payload) ? sizeof(event.payload) : payload.size();
        event.truncated = payload.size() > sizeof(event.payload);
        if (event.payload_length) std::memcpy(event.payload, payload.data(), event.payload_length);
        if (event.truncated) truncated.fetch_add(1);
        queue.push(event);
    }
    void onScanEnd(const NimBLEScanResults&, int reason) override {
        scan_reason.store(reason); scan_ended.store(true, std::memory_order_release);
    }
} callbacks;
class Backend : public RadioBackend {
    bool initialized_ = false;
    NimBLEScan* scan_ = nullptr;
    // NimBLE init waits for host synchronization internally. Keep that wait off
    // the UI/coordinator task so even a stalled initialization restores Wi-Fi.
    std::atomic<int> init_state_{0}; // 0=not started, 1=worker, 2=ready, 3=failed
    static void initialize(void* context) {
        auto* self = static_cast<Backend*>(context);
        // Do not enter NimBLE's erase-on-NVS-error fallback: preserve trust data.
        const bool ok = nvs_flash_init() == ESP_OK && NimBLEDevice::init("");
        self->init_state_.store(ok ? 2 : 3, std::memory_order_release);
        vTaskDelete(nullptr);
    }
public:
    bool wifiStart() override { goblinWifiMonitor().start(); return goblinWifiMonitor().running(); }
    bool wifiStop() override { goblinWifiMonitor().stop(); return !goblinWifiMonitor().running(); }
    bool wifiRunning() const override { return goblinWifiMonitor().running(); }
    bool bleRunning() const override { return scan_ && scan_->isScanning(); }
    bool bleInitializing() const override { return init_state_.load() == 1; }
    bool bleStart(uint32_t duration, uint32_t window) override {
        if (!initialized_) {
            int state = init_state_.load(std::memory_order_acquire);
            if (!state) {
                init_state_.store(1);
                if (xTaskCreate(initialize, "goblin-ble-init", 6144, this, 1, nullptr) != pdPASS) init_state_.store(3);
                Serial.println("[ble] initialization scheduled; Wi-Fi resumes while host syncs");
                return false;
            }
            if (state != 2) {
                Serial.println(state == 3 ? "[ble] init failed; BLE disabled this boot, Wi-Fi retained" : "[ble] host init pending; Wi-Fi retained");
                return false;
            }
            initialized_ = true;
            scan_ = NimBLEDevice::getScan();
            if (!scan_) { Serial.println("[ble] scan allocation failed"); return false; }
            scan_->setScanCallbacks(&callbacks, true);
            scan_->setActiveScan(false); scan_->setMaxResults(0);
            scan_->setInterval(100); scan_->setWindow(80);
        }
        if (!scan_) return false;
        scan_ended.store(false); scan_window.store(window); accepting.store(true, std::memory_order_release);
        if (!scan_->start(duration, false, true)) {
            accepting.store(false); Serial.println("[ble] start failed; restoring Wi-Fi"); return false;
        }
        Serial.printf("[ble] passive scan started window=%u duration_ms=%u\n", window, duration); return true;
    }
    bool bleStop() override {
        accepting.store(false, std::memory_order_release);
        if (!scan_) return true;
        const bool was_running = scan_->isScanning();
        if (!scan_->stop()) { Serial.println("[ble] stop failed; resetting NimBLE"); return false; }
        if (was_running) Serial.println("[ble] scan stopped");
        return !scan_->isScanning();
    }
    bool bleReset() override {
        accepting.store(false);
        // clearAll=true deletes the scan object even when shutdown FAILS.
        // Retain it until a confirmed shutdown to avoid a dangling scan pointer.
        const bool ok = NimBLEDevice::deinit(false);
        Serial.printf("[ble] recovery deinit=%s\n", ok ? "OK" : "FAILED; retry pending");
        if (ok) { NimBLEDevice::deinit(true); scan_ = nullptr; initialized_ = false; init_state_.store(0); }
        return ok;
    }
} backend;
RadioCoordinator coordinator(backend);
}
uint64_t goblinRadioNow() { return static_cast<uint64_t>(esp_timer_get_time()) / 1000; }
RadioCoordinator& goblinRadio() { return coordinator; }
BleDeviceInventory& goblinBleInventory() { return inventory; }
const PrivacyWatchAnalyzer& goblinPrivacy() { return privacy; }
uint32_t goblinBleNotifications() { return notifications; }
uint32_t goblinBleQueueDrops() { return queue.dropped(); }
uint64_t goblinBleObservations() { return observations; }
void goblinBleTick() {
    const uint64_t now = goblinRadioNow();
    privacy.expire(now); goblinGuardAlerts().expireBle(now);
    for (size_t i = 0; i < inventory.count(); ++i) {
        const auto* r = inventory.at(i); const auto* c = privacy.find(r->key);
        inventory.setPersistent(r->key, c && c->persistent);
    }
    BleObservationEvent event;
    for (unsigned i = 0; i < kBleQueueCapacity && queue.pop(event); ++i) {
        ++observations;
        if (now >= event.timestamp_ms && now - event.timestamp_ms >= kPrivacyExpiryMs) continue;
        const auto parsed = parseBleAdvertisement(event.payload, event.payload_length);
        if (parsed.malformed) ++malformed;
        auto* record = inventory.observe(event, parsed);
        if (!record) continue;
        const auto* candidate = privacy.observe(event, parsed.classification);
            if (candidate) {
            record->persistent = candidate->persistent;
            if (goblinGuardAlerts().recordBle(*record, *candidate)) {
                ++notifications;
                goblinLogBleAlert(*record, candidate->persistent);
                Serial.printf("[privacy] %s protocol=%s confidence=%s (observation, not malicious intent)\n",
                    candidate->persistent ? "PERSISTENT TRACKER-CAPABLE NEARBY" : "TRACKER-CAPABLE DEVICE OBSERVED",
                    bleClassName(parsed.classification), bleConfidenceName(parsed.confidence));
            }
        }
    }
    if (scan_ended.exchange(false)) Serial.printf("[ble] window ended reason=%d; coordinator owns restore\n", scan_reason.load());
    coordinator.tick(now);
    static RadioMode previous = RadioMode::Idle;
    static uint32_t last_failures = 0;
    if (previous != coordinator.mode() || last_failures != coordinator.failures()) {
        Serial.printf("[radio] %s -> %s failures=%u\n", radioModeName(previous), radioModeName(coordinator.mode()), coordinator.failures());
        previous = coordinator.mode(); last_failures = coordinator.failures();
    }
    static uint64_t last_diagnostics = 0;
    if (now - last_diagnostics < 5000) return;
    last_diagnostics = now;
    Serial.printf("[ble] mode=%s wifi_ms=%llu ble_ms=%llu observations=%llu inventory=%u queue_drops=%u evictions=%u inventory_drops=%u candidates=%u candidate_evictions=%u candidate_drops=%u truncated=%u malformed=%u failures=%u heap=%u min_heap=%u psram=%u min_psram=%u\n",
        radioModeName(coordinator.mode()), static_cast<unsigned long long>(coordinator.wifiUptime()), static_cast<unsigned long long>(coordinator.bleUptime()),
        static_cast<unsigned long long>(observations), static_cast<unsigned>(inventory.count()), queue.dropped(), inventory.evictions(), inventory.drops(),
        static_cast<unsigned>(privacy.count()), privacy.evictions(), privacy.drops(), truncated.load(), malformed, coordinator.failures(),
        ESP.getFreeHeap(), goblinAirMinHeap(), ESP.getFreePsram(), goblinAirMinPsram());
}
