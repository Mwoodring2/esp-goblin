#pragma once
#include "access_point_inventory.h"

struct WifiSnapshot {
    int accessPoints = 0;
    int openNetworks = 0;
    int strongestRssi = -127;
    int busiestChannel = 0;
    int busiestCount = 0;
    size_t inventoryCount = 0;
    size_t knownCount = 0;
    size_t unknownCount = 0;
    size_t newCount = 0;
    bool scanFailed = false;
    bool inventoryIncomplete = false;
};

WifiSnapshot goblinScanWifi();
AccessPointInventory& goblinApInventory();
