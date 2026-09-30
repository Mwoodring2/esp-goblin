#include "wifi_snapshot.h"
#include <WiFi.h>
#include <esp_timer.h>

AccessPointInventory& goblinApInventory() {
    static AccessPointInventory inventory;
    return inventory;
}

WifiSnapshot goblinScanWifi() {
    WifiSnapshot s;
    AccessPointInventory& inventory = goblinApInventory();
    inventory.beginScan();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(50);

    const int found = WiFi.scanNetworks(false, true);
    if (found < 0) {
        s.scanFailed = true;
        Serial.printf("[guard] Wi-Fi scan failed (%d); inventory retained\n", found);
    }
    const uint64_t now = static_cast<uint64_t>(esp_timer_get_time()) / 1000;

    int channels[15] = {};

    for (int i = 0; i < found; ++i) {
        const uint8_t* bssid = WiFi.BSSID(i);
        const String ssid = WiFi.SSID(i);
        const auto result = inventory.observe(bssid, ssid.c_str(), WiFi.channel(i),
                                               WiFi.RSSI(i), WiFi.encryptionType(i), now);
        if (result == AccessPointInventory::MergeResult::New) {
            Serial.printf("[guard] NEW AP %02X:%02X:%02X:%02X:%02X:%02X ssid=\"%s\" channel=%d rssi=%d auth=%d state=Unknown\n",
                bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                ssid.c_str(), WiFi.channel(i), WiFi.RSSI(i), static_cast<int>(WiFi.encryptionType(i)));
        } else if (result == AccessPointInventory::MergeResult::Invalid ||
                   result == AccessPointInventory::MergeResult::OutOfMemory) {
            s.inventoryIncomplete = true;
            Serial.printf("[guard] Cannot store scan result %d: %s\n", i,
                result == AccessPointInventory::MergeResult::Invalid ? "invalid AP data" : "out of memory");
        }
        ++s.accessPoints;

        const int rssi = WiFi.RSSI(i);
        if (rssi > s.strongestRssi) s.strongestRssi = rssi;

        if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
            ++s.openNetworks;
        }

        const int ch = WiFi.channel(i);
        if (ch >= 1 && ch <= 14) ++channels[ch];
    }

    for (int ch = 1; ch <= 14; ++ch) {
        if (channels[ch] > s.busiestCount) {
            s.busiestCount = channels[ch];
            s.busiestChannel = ch;
        }
    }

    s.inventoryCount = inventory.totalCount();
    s.knownCount = inventory.knownCount();
    s.unknownCount = inventory.unknownCount();
    s.newCount = inventory.newCount();
    Serial.printf("[guard] inventory=%u known=%u unknown=%u new=%u\n",
        static_cast<unsigned>(s.inventoryCount), static_cast<unsigned>(s.knownCount),
        static_cast<unsigned>(s.unknownCount), static_cast<unsigned>(s.newCount));
    WiFi.scanDelete();
    return s;
}
