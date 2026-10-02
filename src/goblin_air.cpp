#include "goblin_air.h"
#include "goblin_events.h"
#include "goblin_guard.h"
#include <Arduino.h>
#include <esp_timer.h>

namespace {
AirMonitor air;
uint32_t last_diagnostics = 0, last_drops = 0, overflow_periods = 0;
uint32_t min_heap = UINT32_MAX, min_psram = UINT32_MAX;
uint64_t verbose_second = 0;
unsigned verbose_count = 0;
}
const AirMonitor& goblinAirMonitor() { return air; }
uint32_t goblinAirMinHeap() { return min_heap == UINT32_MAX ? 0 : min_heap; }
uint32_t goblinAirMinPsram() { return min_psram == UINT32_MAX ? 0 : min_psram; }

void goblinAirTick() {
    auto& radio = goblinWifiMonitor();
    WifiFrameEvent event{};
    for (unsigned i = 0; i < 64 && radio.queue().pop(event); ++i) {
        DisconnectBurst burst;
        if (air.process(event, goblinTrustedStore(), goblinGuardAlerts(), burst)) {
            goblinLogDisconnect(burst, event.subtype);
            char bssid[18], source[18];
            guardFormatBssid(burst.bssid, bssid, sizeof(bssid));
            guardFormatBssid(burst.source, source, sizeof(source));
            Serial.printf("[air] UNUSUAL DISCONNECT ACTIVITY bssid=%s src=%s channel=%u rssi=%d frames=%u%s window_ms=%u trusted=%s reason=%s%u\n",
                bssid, source, burst.channel, burst.rssi, burst.frame_count, burst.count_lower_bound ? "+" : "",
                burst.window_ms, isTrustedAccessPoint(burst.bssid) ? "YES" : "NO",
                burst.has_reason_code ? "" : "unavailable/", burst.reason_code);
        }
        if (kAirVerbose && (event.subtype == 10 || event.subtype == 12)) {
            const uint64_t second = event.timestamp_us / 1000000;
            if (second != verbose_second) { verbose_second = second; verbose_count = 0; }
            if (verbose_count++ < 4) {
                char bssid[18], source[18], destination[18];
                guardFormatBssid(event.bssid, bssid, sizeof(bssid));
                guardFormatBssid(event.source, source, sizeof(source));
                guardFormatBssid(event.destination, destination, sizeof(destination));
                Serial.printf("[air] %s bssid=%s src=%s dst=%s channel=%u rssi=%d reason=%s%u\n",
                    event.subtype == 12 ? "DEAUTH" : "DISASSOC", bssid, source, destination,
                    event.channel, event.rssi, event.has_reason_code ? "" : "unavailable/", event.reason_code);
            }
        }
    }
    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
    air.advance(now_us, radio.queue().dropped());
    radio.tick(millis());
    const uint32_t heap = ESP.getFreeHeap(), psram = ESP.getFreePsram();
    if (heap < min_heap) min_heap = heap;
    if (psram < min_psram) min_psram = psram;
    if (millis() - last_diagnostics < kAirDiagnosticsMs) return;
    last_diagnostics = millis();
    const uint32_t dropped = radio.queue().dropped();
    const uint32_t delta = dropped - last_drops;
    last_drops = dropped;
    overflow_periods = delta ? overflow_periods + 1 : 0;
    if (delta) Serial.printf("[air] WARNING event queue dropped %u frames%s\n", delta,
        overflow_periods >= 3 ? " (sustained overflow)" : "");
    if (radio.running() || air.lifetime().management) {
        Serial.printf("[air] diagnostics heap=%u min_heap=%u psram=%u min_psram=%u queued=%u high_water=%u dropped=%llu processed=%llu rejected=%u channel=%u detector_evictions=%u\n",
            heap, min_heap, psram, min_psram, radio.queue().queued(), radio.queue().highWater(),
            static_cast<unsigned long long>(air.lifetime().queue_drops),
            static_cast<unsigned long long>(air.lifetime().management), radio.rejected(), radio.channel(), air.detectorEvictions());
    }
}
