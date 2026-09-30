#include "wifi_snapshot.h"
#include <WiFi.h>

WifiSnapshot goblinScanWifi() {
    WifiSnapshot s;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(50);

    const int found = WiFi.scanNetworks(false, true);
    if (found <= 0) {
        WiFi.scanDelete();
        return s;
    }

    int channels[15] = {};

    for (int i = 0; i < found; ++i) {
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

    WiFi.scanDelete();
    return s;
}
