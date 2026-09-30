#include "security_snapshot.h"

#include <WiFi.h>

WifiSnapshot scanWifiSnapshot() {
    WifiSnapshot snapshot;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(50);

    // Passive-only monitoring comes later. This first milestone intentionally
    // uses the normal ESP32 network scan API to validate radio + UI plumbing.
    const int count = WiFi.scanNetworks(false, true);

    if (count <= 0) {
        WiFi.scanDelete();
        return snapshot;
    }

    int channel_counts[15] = {0};

    for (int i = 0; i < count; ++i) {
        const int channel = WiFi.channel(i);
        const int rssi = WiFi.RSSI(i);

        snapshot.access_point_count += 1;

        if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
            snapshot.open_network_count += 1;
        }

        if (rssi > snapshot.strongest_rssi) {
            snapshot.strongest_rssi = rssi;
        }

        if (channel >= 1 && channel <= 14) {
            channel_counts[channel] += 1;
        }
    }

    for (int channel = 1; channel <= 14; ++channel) {
        if (channel_counts[channel] > snapshot.busiest_channel_count) {
            snapshot.busiest_channel = channel;
            snapshot.busiest_channel_count = channel_counts[channel];
        }
    }

    WiFi.scanDelete();
    return snapshot;
}
