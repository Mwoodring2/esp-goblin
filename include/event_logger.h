#pragma once
#include "event_record.h"

enum class LogResult : uint8_t { Queued, Suppressed, Dropped };

class EventDeduplicator {
public:
    bool blocked(EventType type, const char* identity, DedupMode mode, uint32_t now_ms, uint32_t cooldown_ms) const;
    void remember(EventType type, const char* identity, uint32_t now_ms);
private:
    struct Slot {
        uint16_t type = 0;
        uint32_t last_ms = 0;
        char identity[kDedupIdentityLength] = {};
        bool used = false;
    };
    Slot slots_[kDedupSlots];
};

class EventLogger {
public:
    LogResult submit(EventRecord& event);
    bool peek(EventRecord& out) const;
    void pop();
    uint32_t dropped() const { return dropped_; }
    uint32_t highWater() const { return high_water_; }
    uint32_t queued() const { return static_cast<uint32_t>(count_); }
    uint32_t nextId() const { return next_id_; }
private:
    EventRecord queue_[kEventQueueCapacity];
    EventDeduplicator dedup_;
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t next_id_ = 1;
    uint32_t dropped_ = 0;
    uint32_t high_water_ = 0;
};

class EventHistory {
public:
    void push(const EventRecord& event);
    size_t count() const { return count_; }
    const EventRecord* newest(size_t offset) const;
private:
    EventRecord items_[kHistoryCapacity];
    size_t start_ = 0;
    size_t count_ = 0;
};
