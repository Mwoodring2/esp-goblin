// Host test: c++ -std=c++11 -Iinclude src/access_point_inventory.cpp
//           tests/access_point_inventory_test.cpp -o inventory_test
#include "access_point_inventory.h"
#include <cstdio>
#include <cstring>

static int failures = 0;
static void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main() {
    AccessPointInventory inventory;
    using Result = AccessPointInventory::MergeResult;
    const uint8_t a[6] = {2, 0, 0, 0, 0, 1};
    const uint8_t b[6] = {2, 0, 0, 0, 0, 2};
    inventory.beginScan();
    check(inventory.observe(a, "shared", 1, -70, 3, 100) == Result::New, "first BSSID is new");
    check(inventory.observe(b, "shared", 6, -50, 0, 100) == Result::New, "same SSID, distinct BSSID is new");
    check(inventory.observe(a, "shared", 1, -60, 3, 100) == Result::Existing, "duplicate BSSID merges");
    check(inventory.totalCount() == 2 && inventory.newCount() == 2 &&
          inventory.unknownCount() == 2 && inventory.knownCount() == 0, "all new APs unknown");
    const auto* record = inventory.find(a);
    check(record && record->seen_count == 1 && record->state == AccessPointState::Unknown,
          "duplicate scan entries count once");
    check(inventory.setKnown(a, true) && inventory.setKnown(a, true), "classification is idempotent");
    inventory.beginScan();
    check(inventory.observe(a, "renamed", 11, -40, 4, 500) == Result::Existing, "rename is not a new AP");
    record = inventory.find(a);
    check(record && record->first_seen == 100 && record->last_seen == 500 &&
          record->seen_count == 2 && record->channel == 11 && record->rssi == -40 &&
          record->auth_mode == 4 && std::strcmp(record->ssid, "renamed") == 0 &&
          record->state == AccessPointState::Known, "refresh metadata, preserve first seen and trust");
    check(inventory.totalCount() == 2 && inventory.newCount() == 0 &&
          inventory.knownCount() == 1 && inventory.unknownCount() == 1, "missing AP retained");
    inventory.beginScan();
    check(inventory.totalCount() == 2 && inventory.newCount() == 0, "empty scan retains inventory");
    check(inventory.observe(nullptr, "bad", 1, -50, 0, 600) == Result::Invalid &&
          inventory.observe(a, nullptr, 1, -50, 0, 600) == Result::Invalid,
          "invalid data rejected");
    check(!inventory.setKnown(nullptr, true) && inventory.totalCount() == 2,
          "invalid classification does not change inventory");
    check(inventory.observe(b, "", 6, -55, 0, 0x100000000ULL) == Result::Existing,
          "hidden SSID updates by BSSID");
    record = inventory.find(b);
    check(record && record->ssid[0] == '\0' && record->last_seen == 0x100000000ULL &&
          record->first_seen == 100 && record->seen_count == 2, "64-bit uptime and hidden SSID");
    check(inventory.setKnown(a, false) && inventory.knownCount() == 0 &&
          inventory.unknownCount() == 2, "explicitly return AP to unknown");
    if (failures) return 1;
    std::puts("PASS: AP inventory identity, rescans, metadata, counts, state, and invalid input");
    return 0;
}
