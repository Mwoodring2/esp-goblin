#include "wifi_promiscuous_monitor.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_timer.h>

namespace { WifiPromiscuousMonitor monitor; }
WifiPromiscuousMonitor& goblinWifiMonitor() { return monitor; }

bool WifiPromiscuousMonitor::checked(esp_err_t result, const char* operation) {
    if (result == ESP_OK) return true;
    failed_ = true;
    Serial.printf("[air] %s failed: %s\n", operation, esp_err_to_name(result));
    return false;
}

bool WifiPromiscuousMonitor::start() {
    if (healthy()) return true;
    if (running_) { stop(); if (running_) return false; }
    failed_ = false;
    Serial.println("[air] promiscuous monitor starting");
    if (!queue_.lockFree() || ATOMIC_BOOL_LOCK_FREE != 2) {
        failed_ = true; Serial.println("[air] lock-free atomics unavailable; monitor disabled"); return false;
    }
    // Arduino initializes/starts the driver, but this firmware never calls begin()
    // with credentials. NULL mode has no station/AP traffic during monitoring.
    if (!WiFi.setAutoReconnect(false)) {
        failed_ = true; Serial.println("[air] Arduino Wi-Fi initialization failed"); return false;
    }
    wifi_mode_t mode;
    const esp_err_t mode_result = esp_wifi_get_mode(&mode);
    if (mode_result == ESP_ERR_WIFI_NOT_INIT) {
        if (!WiFi.mode(WIFI_STA)) {
            failed_ = true; Serial.println("[air] Arduino Wi-Fi driver initialization failed"); return false;
        }
    } else if (!checked(mode_result, "esp_wifi_get_mode")) return false;
    if (!checked(esp_wifi_set_mode(WIFI_MODE_NULL), "esp_wifi_set_mode(NULL)")) return false;
    wifi_country_t country{};
    if (!checked(esp_wifi_get_country(&country), "esp_wifi_get_country")) return false;
    if (!scheduler_.configure(country.schan, country.nchan, millis())) {
        failed_ = true; Serial.println("[air] invalid country channel range; monitor disabled"); return false;
    }
    Serial.printf("[air] configured country %.3s channels %u-%u\n", country.cc,
        country.schan, country.schan + country.nchan - 1);
    wifi_promiscuous_filter_t filter{};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    if (!checked(esp_wifi_set_promiscuous_filter(&filter), "esp_wifi_set_promiscuous_filter")) return false;
    Serial.println("[air] management filter installed");
    if (!checked(esp_wifi_set_promiscuous_rx_cb(receive), "esp_wifi_set_promiscuous_rx_cb") ||
        !checked(esp_wifi_set_channel(scheduler_.channel(), WIFI_SECOND_CHAN_NONE), "esp_wifi_set_channel")) return false;
    accepting_.store(true, std::memory_order_release);
    if (!checked(esp_wifi_set_promiscuous(true), "esp_wifi_set_promiscuous(true)")) {
        accepting_.store(false, std::memory_order_release);
        // Conservatively mark the radio occupied if cleanup also fails.
        running_ = !checked(esp_wifi_set_promiscuous(false), "esp_wifi_set_promiscuous(cleanup)");
        return false;
    }
    running_ = true;
    Serial.println("[air] monitor running");
    return true;
}

bool WifiPromiscuousMonitor::prepareForScan() {
    if (running_) { Serial.println("[air] scan preparation refused while monitor running"); return false; }
    wifi_mode_t mode;
    const esp_err_t result = esp_wifi_get_mode(&mode);
    if (result == ESP_ERR_WIFI_NOT_INIT) return true; // normal scan initializes Arduino
    if (!checked(result, "esp_wifi_get_mode(scan)")) return false;
    // Avoid repeatedly registering Arduino network handlers when returning from
    // raw IDF NULL mode. Arduino already owns the initialized STA interface.
    return mode != WIFI_MODE_NULL || checked(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode(scan STA)");
}

void WifiPromiscuousMonitor::stop() {
    accepting_.store(false, std::memory_order_release);
    if (!running_) return;
    if (!checked(esp_wifi_set_promiscuous(false), "esp_wifi_set_promiscuous(false)")) return;
    running_ = false;
    Serial.println("[air] monitor stopped/paused");
    // Keep queue storage alive; an already-entered callback may finish safely.
}

void WifiPromiscuousMonitor::tick(uint32_t now_ms) {
    if (!healthy() || !scheduler_.due(now_ms)) return;
    wifi_country_t country{};
    if (!checked(esp_wifi_get_country(&country), "esp_wifi_get_country")) { stop(); return; }
    if (country.schan != scheduler_.first() || country.nchan != scheduler_.count()) {
        // Reinitialize on a configuration change rather than hop outside it.
        stop(); if (!running_) start(); return;
    }
    const uint8_t next = scheduler_.next();
    if (!checked(esp_wifi_set_channel(next, WIFI_SECOND_CHAN_NONE), "esp_wifi_set_channel(hop)")) { stop(); return; }
    scheduler_.advanced(now_ms);
    if (kAirVerbose) Serial.printf("[air] channel %u\n", next);
}

void WifiPromiscuousMonitor::receive(void* buffer, wifi_promiscuous_pkt_type_t type) {
    if (!monitor.accepting_.load(std::memory_order_acquire) || !buffer || type != WIFI_PKT_MGMT) return;
    const auto* packet = static_cast<const wifi_promiscuous_pkt_t*>(buffer);
    WifiFrameEvent event{};
    // ESP-IDF sig_len includes a four-byte FCS; parser only receives body length.
    if (packet->rx_ctrl.rx_state != 0 || packet->rx_ctrl.sig_len < 28 ||
        !parseWifiManagementFrame(packet->payload, packet->rx_ctrl.sig_len - 4,
            static_cast<uint64_t>(esp_timer_get_time()), packet->rx_ctrl.rssi, packet->rx_ctrl.channel, event)) {
        monitor.rejected_.store(monitor.rejected_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        return;
    }
    monitor.queue_.push(event);
}
