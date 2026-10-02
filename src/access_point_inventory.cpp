#include "access_point_inventory.h"

#include <cstring>
#include <new>

const AccessPointRecord* AccessPointInventory::at(size_t index) const {
    const Node* node = head_;
    while (node && index--) node = node->next;
    return node ? &node->record : nullptr;
}

bool AccessPointInventory::seenThisScan(const uint8_t* bssid) const {
    const Node* node = findNode(bssid);
    return node && node->seen_this_scan;
}

AccessPointInventory::~AccessPointInventory() {
    while (head_) {
        Node* next = head_->next;
        delete head_;
        head_ = next;
    }
}

AccessPointInventory::Node* AccessPointInventory::findNode(const uint8_t* bssid) const {
    if (!bssid) return nullptr;
    for (Node* node = head_; node; node = node->next) {
        if (std::memcmp(node->record.bssid, bssid, 6) == 0) return node;
    }
    return nullptr;
}

const AccessPointRecord* AccessPointInventory::find(const uint8_t* bssid) const {
    const Node* node = findNode(bssid);
    return node ? &node->record : nullptr;
}

void AccessPointInventory::beginScan() {
    new_ = 0;
    for (Node* node = head_; node; node = node->next) node->seen_this_scan = false;
}

AccessPointInventory::MergeResult AccessPointInventory::observe(
    const uint8_t* bssid, const char* ssid, int channel, int rssi,
    int auth_mode, uint64_t now) {
    if (!bssid || !ssid) return MergeResult::Invalid;
    Node* node = findNode(bssid);
    const bool is_new = node == nullptr;
    if (is_new) {
        node = new (std::nothrow) Node;
        if (!node) return MergeResult::OutOfMemory;
        std::memcpy(node->record.bssid, bssid, 6);
        node->record.first_seen = now;
        node->next = head_;
        head_ = node;
        ++total_;
        ++new_;
    }
    AccessPointRecord& record = node->record;
    std::strncpy(record.ssid, ssid, sizeof(record.ssid) - 1);
    record.ssid[sizeof(record.ssid) - 1] = '\0';
    record.channel = channel;
    record.rssi = rssi;
    record.auth_mode = auth_mode;
    record.last_seen = now;
    if (!node->seen_this_scan && record.seen_count < UINT32_MAX) ++record.seen_count;
    node->seen_this_scan = true;
    return is_new ? MergeResult::New : MergeResult::Existing;
}

bool AccessPointInventory::setKnown(const uint8_t* bssid, bool known) {
    Node* node = findNode(bssid);
    if (!node) return false;
    const auto state = known ? AccessPointState::Known : AccessPointState::Unknown;
    if (node->record.state != state) {
        if (known) ++known_;
        else --known_;
        node->record.state = state;
    }
    return true;
}
