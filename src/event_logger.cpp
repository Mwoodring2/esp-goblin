#include "event_logger.h"
#include <cstring>

bool EventDeduplicator::blocked(EventType type, const char* identity, DedupMode mode,
                                uint32_t now_ms, uint32_t cooldown_ms) const {
    if (!identity || mode == DedupMode::None) return false;
    for (size_t i = 0; i < kDedupSlots; ++i) {
        const Slot& slot = slots_[i];
        if (!slot.used || slot.type != static_cast<uint16_t>(type) || std::strcmp(slot.identity, identity)) continue;
        if (mode == DedupMode::Once) return true;
        return now_ms - slot.last_ms < cooldown_ms;
    }
    return false;
}
void EventDeduplicator::remember(EventType type, const char* identity, uint32_t now_ms) {
    if (!identity) return;
    Slot* match = 0;
    Slot* empty = 0;
    Slot* oldest = 0;
    for (size_t i = 0; i < kDedupSlots; ++i) {
        Slot& slot = slots_[i];
        if (slot.used && slot.type == static_cast<uint16_t>(type) && !std::strcmp(slot.identity, identity)) {
            match = &slot;
            break;
        }
        if (!slot.used) { if (!empty) empty = &slot; }
        else if (!oldest || slot.last_ms < oldest->last_ms) oldest = &slot;
    }
    Slot* chosen = match ? match : (empty ? empty : oldest);
    if (!chosen) return;
    chosen->used = true;
    chosen->type = static_cast<uint16_t>(type);
    chosen->last_ms = now_ms;
    std::strncpy(chosen->identity, identity, kDedupIdentityLength - 1);
    chosen->identity[kDedupIdentityLength - 1] = 0;
}
LogResult EventLogger::submit(EventRecord& event) {
    char identity[kDedupIdentityLength];
    eventDedupIdentity(event, identity, sizeof(identity));
    const DedupMode mode = eventDedupMode(event.type);
    if (dedup_.blocked(event.type, identity, mode, event.monotonic_ms, kDedupCooldownMs)) return LogResult::Suppressed;
    if (count_ >= kEventQueueCapacity) {
        ++dropped_;
        return LogResult::Dropped;
    }
    if (mode != DedupMode::None) dedup_.remember(event.type, identity, event.monotonic_ms);
    event.schema = kEventSchemaVersion;
    event.event_id = next_id_++;
    queue_[(head_ + count_) % kEventQueueCapacity] = event;
    ++count_;
    if (count_ > high_water_) high_water_ = static_cast<uint32_t>(count_);
    return LogResult::Queued;
}
bool EventLogger::peek(EventRecord& out) const {
    if (!count_) return false;
    out = queue_[head_];
    return true;
}
void EventLogger::pop() {
    if (!count_) return;
    head_ = (head_ + 1) % kEventQueueCapacity;
    --count_;
}
void EventHistory::push(const EventRecord& event) {
    if (count_ < kHistoryCapacity) {
        items_[(start_ + count_) % kHistoryCapacity] = event;
        ++count_;
        return;
    }
    items_[start_] = event;
    start_ = (start_ + 1) % kHistoryCapacity;
}
const EventRecord* EventHistory::newest(size_t offset) const {
    if (offset >= count_) return 0;
    const size_t index = (start_ + count_ - 1 - offset) % kHistoryCapacity;
    return &items_[index];
}
