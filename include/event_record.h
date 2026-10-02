#pragma once
#include "event_config.h"

enum class EventCategory : uint8_t {
    System, Storage, Guard, Wifi, Ble, Privacy, User, Configuration
};
enum class EventSeverity : uint8_t { Info, Notice, Warning, Critical };
enum class EventType : uint8_t {
    Boot, SessionStart, SessionEnd, TimeSync, LowMemoryWarning, UnexpectedRestart,
    PreviousLogTruncated, SdMounted, SdRemoved, SdMountFailed, SdWriteFailed, SdLowSpace,
    LogQueueOverflow, LogRecovery, NewAccessPoint, TrustedApAdded, TrustedApRemoved,
    TrustedSsidChanged, TrustedSecurityChanged, TrustedChannelChanged, SsidCollision,
    DeauthBurst, DisassocBurst, AirMonitorStarted, AirMonitorStopped, AirSummary,
    BleTrackerCapableObserved, BlePersistentTrackerCandidate, BlePrivacyAlertDismissed,
    BleSummary, AlertDismissed, UserMarkedApTrusted, UserForgotAp, ConfigurationChanged,
    RadioError
};
enum class DedupMode : uint8_t { None, Cooldown, Once };

struct SessionFacts {
    const char* firmware;
    const char* board_id;
    const char* reset_reason;
    const char* sd_card_type;
    uint32_t flash_size;
    uint32_t psram_size;
    uint32_t free_heap;
    uint32_t free_psram;
    uint64_t sd_size;
    uint32_t trusted_ap_count;
    bool wifi_monitor_enabled;
    bool ble_privacy_enabled;
};

struct EventRecord {
    uint32_t session_id = 0;
    uint32_t event_id = 0;
    uint32_t monotonic_ms = 0;
    uint32_t count = 0;
    uint32_t span_ms = 0;
    uint32_t metric[8] = {};
    uint64_t unix_ms = 0;
    int16_t rssi = 0;
    int16_t channel = 0;
    int16_t reason_code = 0;
    EventCategory category = EventCategory::System;
    EventType type = EventType::Boot;
    EventSeverity severity = EventSeverity::Info;
    uint8_t schema = kEventSchemaVersion;
    uint16_t flags = 0;
    char summary[80] = {};
    char ssid[33] = {};
    char bssid[18] = {};
    char trusted_bssid[18] = {};
    char ble_address[18] = {};
    char ble_name[33] = {};
    char old_value[40] = {};
    char new_value[40] = {};
    char source[24] = {};
};

const char* eventCategoryName(EventCategory category);
const char* eventTypeName(EventType type);
const char* eventShortName(EventType type);
const char* eventSeverityName(EventSeverity severity);
EventSeverity eventDefaultSeverity(EventType type);
DedupMode eventDedupMode(EventType type);
void eventCopyText(char* dest, size_t capacity, const char* source, uint16_t& flags);
void eventDedupIdentity(const EventRecord& event, char* out, size_t capacity);
bool eventToJsonLine(const EventRecord& event, const SessionFacts* facts, char* out, size_t capacity, size_t& length);
bool eventFromJsonLine(const char* line, EventRecord& event);
