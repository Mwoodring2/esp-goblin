#include "goblin_guard.h"
#include "goblin_events.h"
#include "wifi_snapshot.h"
#include <Arduino.h>
#include <Preferences.h>
#include <nvs.h>
#include <cstdio>

namespace {
class NvsBaselineStorage : public TrustedApStorage {
public:
    ReadResult read(uint8_t* data, size_t capacity, size_t& size) override {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("goblin_guard", NVS_READONLY, &handle);
        if (err == ESP_ERR_NVS_NOT_FOUND) return ReadResult::Missing;
        if (err != ESP_OK) {
            Serial.printf("[guard] baseline load: NVS open failed (%d)\n", err);
            return ReadResult::Error;
        }
        size = 0;
        err = nvs_get_blob(handle, "baseline", nullptr, &size);
        if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); return ReadResult::Missing; }
        if (err != ESP_OK || size > capacity || size == 0) {
            Serial.printf("[guard] baseline load: invalid blob length/type or read error (%d)\n", err);
            nvs_close(handle);
            return ReadResult::Error;
        }
        err = nvs_get_blob(handle, "baseline", data, &size);
        nvs_close(handle);
        if (err != ESP_OK) Serial.printf("[guard] baseline read failed (%d)\n", err);
        return err == ESP_OK ? ReadResult::Ok : ReadResult::Error;
    }
    bool write(const uint8_t* data, size_t size) override {
        Preferences preferences;
        if (!preferences.begin("goblin_guard", false)) {
            Serial.println("[guard] baseline save failed: cannot open NVS");
            return false;
        }
        const size_t written = preferences.putBytes("baseline", data, size);
        preferences.end();
        if (written != size) {
            Serial.printf("[guard] baseline save failed: wrote %u/%u bytes\n",
                static_cast<unsigned>(written), static_cast<unsigned>(size));
            return false;
        }
        return true;
    }
};
NvsBaselineStorage storage;
TrustedApStore baseline(storage);
GuardAnalyzer analyzer;
}

void guardFormatBssid(const uint8_t* bssid, char* output, size_t size) {
    if (!output || !size) return;
    if (!bssid) { output[0] = 0; return; }
    std::snprintf(output, size, "%02X:%02X:%02X:%02X:%02X:%02X",
        bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}
const TrustedApStore& goblinTrustedStore() { return baseline; }
GuardAnalyzer& goblinGuardAlerts() { return analyzer; }
bool isTrustedAccessPoint(const uint8_t* bssid) { return baseline.isTrustedAccessPoint(bssid); }

bool configureTrustedAccessPoint(const uint8_t* bssid, const char* name, bool enabled) {
    if (!baseline.configure(bssid, name, enabled)) {
        Serial.printf("[guard] baseline configuration failed: %s\n", baseline.errorText());
        return false;
    }
    char address[18]; guardFormatBssid(bssid, address, sizeof(address));
    Serial.printf("[guard] baseline configuration saved bssid=%s enabled=%d\n", address, enabled);
    goblinLogConfig(enabled ? "trusted AP enabled" : "trusted AP disabled");
    goblinGuardAnalyze(false);
    return true;
}

void goblinGuardBegin() {
    if (baseline.load())
        Serial.printf("[guard] baseline loaded: %u trusted APs\n", static_cast<unsigned>(baseline.count()));
    else Serial.printf("[guard] baseline load failed: %s; boot continues, trust edits disabled\n", baseline.errorText());
}

void goblinGuardAnalyze(bool include_new_events) {
    auto& inventory = goblinApInventory();
    analyzer.beginScan();
    for (size_t i = 0; i < inventory.totalCount(); ++i) {
        const auto* ap = inventory.at(i);
        inventory.setKnown(ap->bssid, baseline.isTrustedAccessPoint(ap->bssid));
        if (inventory.seenThisScan(ap->bssid))
            analyzer.observe(*ap, baseline, include_new_events && ap->seen_count == 1);
    }
    for (size_t i = 0; i < analyzer.count(); ++i) {
        const auto* alert = analyzer.at(i);
        if (guardBleAlert(alert->type) || alert->type == GuardAlertType::NewAccessPoint || alert->type == GuardAlertType::DisconnectBurst) continue;
        char observed[18], trusted[18];
        guardFormatBssid(alert->wifi.bssid, observed, sizeof(observed));
        guardFormatBssid(alert->wifi.trusted_bssid, trusted, sizeof(trusted));
        Serial.printf("[guard] %s trusted_bssid=%s observed_bssid=%s ssid=\"%s\" old=\"%s\" new=\"%s\" rssi=%d channel=%d auth=%s (%d)\n",
            guardAlertName(alert->type), trusted, observed, alert->wifi.ssid, alert->wifi.old_value,
            alert->wifi.new_value, alert->wifi.rssi, alert->wifi.channel, guardSecurityName(alert->wifi.auth_mode), alert->wifi.auth_mode);
    }
    goblinLogGuardAlerts();
    if (analyzer.droppedCount()) Serial.printf("[guard] alert capacity reached: %u events omitted; security changes prioritized\n",
        static_cast<unsigned>(analyzer.droppedCount()));
}

bool trustAccessPoint(const uint8_t* bssid) {
    const auto* ap = goblinApInventory().find(bssid);
    if (!ap) { Serial.println("[guard] trust failed: BSSID not in inventory"); return false; }
    const bool already = baseline.isTrustedAccessPoint(bssid);
    if (!baseline.trust(*ap)) {
        Serial.printf("[guard] trust failed: %s\n", baseline.errorText());
        return false;
    }
    char address[18]; guardFormatBssid(bssid, address, sizeof(address));
    Serial.printf("[guard] trusted AP %s bssid=%s ssid=\"%s\"\n", already ? "unchanged" : "added", address, ap->ssid);
    goblinLogTrust(!already, bssid, ap->ssid);
    goblinGuardAnalyze(false);
    return true;
}
bool untrustAccessPoint(const uint8_t* bssid) {
    if (!baseline.untrust(bssid)) {
        Serial.printf("[guard] untrust failed: %s\n", baseline.errorText());
        return false;
    }
    char address[18]; guardFormatBssid(bssid, address, sizeof(address));
    Serial.printf("[guard] trusted AP removed bssid=%s\n", address);
    const auto* forgotten = goblinApInventory().find(bssid);
    goblinLogForget(bssid, forgotten ? forgotten->ssid : "");
    goblinGuardAnalyze(false);
    return true;
}
