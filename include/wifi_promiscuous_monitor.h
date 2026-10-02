#pragma once
#include "wifi_frame_queue.h"
#include "channel_scheduler.h"
#include <esp_wifi.h>

class WifiPromiscuousMonitor : public WifiMonitorControl {
public:
    bool start() override;
    void stop() override;
    bool running() const override { return running_; }
    void tick(uint32_t now_ms);
    bool prepareForScan(); // only after a successful pause
    bool healthy() const { return running_ && accepting_.load(std::memory_order_relaxed); }
    bool failed() const { return failed_; }
    uint8_t channel() const { return scheduler_.channel(); }
    uint8_t firstChannel() const { return scheduler_.first(); }
    uint8_t channelCount() const { return scheduler_.count(); }
    uint32_t rejected() const { return rejected_.load(std::memory_order_relaxed); }
    WifiFrameQueue& queue() { return queue_; }
private:
    static void receive(void* buffer, wifi_promiscuous_pkt_type_t type);
    bool checked(esp_err_t result, const char* operation);
    WifiFrameQueue queue_;
    ChannelScheduler scheduler_;
    std::atomic<bool> accepting_{false};
    std::atomic<uint32_t> rejected_{0};
    bool running_ = false, failed_ = false;
};

WifiPromiscuousMonitor& goblinWifiMonitor();
