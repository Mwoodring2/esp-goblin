#pragma once

struct WifiSnapshot {
    int accessPoints = 0;
    int openNetworks = 0;
    int strongestRssi = -127;
    int busiestChannel = 0;
    int busiestCount = 0;
};

WifiSnapshot goblinScanWifi();
