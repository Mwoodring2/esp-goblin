#pragma once

#include <Arduino.h>

struct WifiSnapshot {
    int access_point_count = 0;
    int open_network_count = 0;
    int strongest_rssi = -127;
    int busiest_channel = 0;
    int busiest_channel_count = 0;
};

WifiSnapshot scanWifiSnapshot();
