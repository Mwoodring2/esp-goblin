#pragma once
#include "wifi_frame_event.h"
#include <atomic>

// One driver-task producer, one main-loop consumer. Release/acquire publishes
// each entire POD slot. No mutex, allocation, spin loop or compare-exchange.
class WifiFrameQueue {
public:
    static constexpr uint32_t Capacity = 128;
    bool push(const WifiFrameEvent& event) {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (used >= Capacity) {
            dropped_.store(dropped_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
            return false;
        }
        events_[head % Capacity] = event;
        head_.store(head + 1, std::memory_order_release);
        if (used + 1 > high_water_.load(std::memory_order_relaxed))
            high_water_.store(used + 1, std::memory_order_relaxed);
        return true;
    }
    bool pop(WifiFrameEvent& event) {
        const uint32_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        event = events_[tail % Capacity];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }
    uint32_t queued() const {
        const uint32_t tail = tail_.load(std::memory_order_acquire);
        const uint32_t count = head_.load(std::memory_order_acquire) - tail;
        return count > Capacity ? Capacity : count;
    }
    uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
    uint32_t highWater() const { return high_water_.load(std::memory_order_relaxed); }
    bool lockFree() const {
        // Capability constants avoid an unavailable libatomic runtime helper on
        // the ESP32 toolchain. Both supported uint32_t underlying types must be lock-free.
        return sizeof(uint32_t) == 4 && ATOMIC_INT_LOCK_FREE == 2 && ATOMIC_LONG_LOCK_FREE == 2;
    }
    // Only while BOTH producer and consumer are quiescent. Not called during
    // radio pause/resume: those preserve lifetime diagnostics and pending events.
    void reset() { head_ = 0; tail_ = 0; dropped_ = 0; high_water_ = 0; }
private:
    WifiFrameEvent events_[Capacity] = {};
    std::atomic<uint32_t> head_{0}, tail_{0}, dropped_{0}, high_water_{0};
};
