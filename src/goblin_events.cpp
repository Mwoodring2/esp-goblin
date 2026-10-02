#include "goblin_events.h"
#include "event_logger.h"
#include "storage_manager.h"
#include "goblin_guard.h"
#include "goblin_air.h"
#include "goblin_ble.h"
#include "app_version.h"
#include "es3c28p_board.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <sys/time.h>
#include <cstdio>
#include <cstring>

void goblinShutdownHook();

namespace {
EventLogger logger;
EventHistory history;
SessionFacts facts = {};
char firmware_text[24] = {};
char board_text[32] = {};
char reset_text[24] = {};
char action_text[48] = "idle";
uint32_t session_id = 0;
uint32_t session_origin_ms = 0;
uint32_t events_written = 0;
uint32_t write_failures = 0;
uint32_t flush_failures = 0;
uint32_t pending_flush = 0;
uint32_t last_flush_ms = 0;
uint32_t last_summary_ms = 0;
uint32_t last_radio_failures = 0;
uint32_t min_heap = UINT32_MAX;
uint32_t min_psram = UINT32_MAX;
uint64_t bytes_written = 0;
bool logging_on = true;
bool summaries_on = true;
bool retention_on = true;
bool checksum_on = true;
bool counter_persisted = false;
bool time_synced = false;
bool low_space_warned = false;
bool storage_is_low = false;
bool overflow_pending = false;
bool write_failed_noted = false;
bool monitor_was = false;
bool session_closed = false;
bool pumping = false;
int reset_code = 0;

class PreferenceSessionStore : public SessionIdStore {
public:
    bool readNext(uint32_t& next) override {
        Preferences preferences;
        if (!preferences.begin("goblin_log", false)) return false;
        next = preferences.getUInt("next", 1);
        preferences.end();
        return true;
    }
    bool writeNext(uint32_t next) override {
        Preferences preferences;
        if (!preferences.begin("goblin_log", false)) return false;
        const size_t wrote = preferences.putUInt("next", next);
        preferences.end();
        return wrote == sizeof(uint32_t);
    }
};

void loadSettings() {
    Preferences preferences;
    if (!preferences.begin("goblin_log", true)) {
        Serial.println("[log] settings unavailable; using defaults");
        return;
    }
    const uint32_t flags = preferences.getUInt("flags", 0x0Fu);
    preferences.end();
    logging_on = flags & 1u;
    summaries_on = flags & 2u;
    retention_on = flags & 4u;
    checksum_on = flags & 8u;
}
void saveSettings() {
    Preferences preferences;
    if (!preferences.begin("goblin_log", false)) {
        Serial.println("[log] settings save failed");
        return;
    }
    const uint32_t flags = (logging_on ? 1u : 0u) | (summaries_on ? 2u : 0u) |
                           (retention_on ? 4u : 0u) | (checksum_on ? 8u : 0u);
    if (preferences.putUInt("flags", flags) != sizeof(uint32_t)) Serial.println("[log] settings write failed");
    preferences.end();
}
void stamp(EventRecord& event) {
    event.session_id = session_id;
    event.monotonic_ms = millis();
    timeval time_value;
    if (gettimeofday(&time_value, 0) == 0 && time_value.tv_sec > 0) {
        const uint64_t unix_ms = static_cast<uint64_t>(time_value.tv_sec) * 1000ull +
                                 static_cast<uint64_t>(time_value.tv_usec) / 1000ull;
        if (wallClockPlausible(unix_ms)) {
            event.unix_ms = unix_ms;
            event.flags = static_cast<uint16_t>(event.flags | kFlagTimeValid);
        }
    }
}
void fill(EventRecord& event, EventType type, EventCategory category, const char* summary) {
    event.type = type;
    event.category = category;
    event.severity = eventDefaultSeverity(type);
    eventCopyText(event.summary, sizeof(event.summary), summary, event.flags);
}
void enqueue(EventRecord event, bool allow_clock_sync);
void drain(bool force);
void enqueue(EventRecord event, bool allow_clock_sync) {
    stamp(event);
    if (allow_clock_sync && (event.flags & kFlagTimeValid) && !time_synced && event.type != EventType::TimeSync) {
        time_synced = true;
        EventRecord sync;
        fill(sync, EventType::TimeSync, EventCategory::System, "UTC time is available");
        eventCopyText(sync.source, sizeof(sync.source), "system", sync.flags);
        enqueue(sync, false);
    } else if (event.flags & kFlagTimeValid) time_synced = true;
    const LogResult result = logger.submit(event);
    if (result == LogResult::Queued) history.push(event);
    else if (result == LogResult::Dropped) {
        overflow_pending = true;
        if (logger.dropped() == 1 || (logger.dropped() % 16u) == 0)
            Serial.printf("[log] event queue full dropped=%lu\n", static_cast<unsigned long>(logger.dropped()));
    }
}
void noteOverflow() {
    if (!overflow_pending || logger.queued() >= kEventQueueCapacity) return;
    overflow_pending = false;
    EventRecord event;
    char summary[80];
    std::snprintf(summary, sizeof(summary), "dropped %lu events while the log queue was full",
                  static_cast<unsigned long>(logger.dropped()));
    fill(event, EventType::LogQueueOverflow, EventCategory::Storage, summary);
    enqueue(event, true);
}
void drain(bool force) {
    if (session_closed) return;
    noteOverflow();
    if (!logging_on) {
        EventRecord ignored;
        while (logger.peek(ignored)) logger.pop();
        return;
    }
    StorageManager& storage = goblinStorage();
    if (!storage.ready()) return;
    if (!storage.sessionOpen() && !storage.openSession(session_id)) {
        ++write_failures;
        return;
    }
    static char line[kJsonLineCapacity];
    bool important = false;
    EventRecord event;
    while (logger.peek(event)) {
        size_t length = 0;
        const SessionFacts* encoded = event.type == EventType::SessionStart ? &facts : 0;
        if (!eventToJsonLine(event, encoded, line, sizeof(line), length)) {
            logger.pop();
            ++write_failures;
            Serial.println("[log] event did not fit the serialization buffer");
            continue;
        }
        if (!storage.writeSession(line, length)) {
            ++write_failures;
            if (!write_failed_noted) {
                write_failed_noted = true;
                EventRecord failed;
                fill(failed, EventType::SdWriteFailed, EventCategory::Storage, "SD write failed");
                enqueue(failed, true);
            }
            break;
        }
        logger.pop();
        ++events_written;
        bytes_written += length;
        ++pending_flush;
        if (event.severity == EventSeverity::Warning || event.severity == EventSeverity::Critical) important = true;
    }
    if (logFlushDue(millis(), last_flush_ms, pending_flush, important, force)) {
        if (!storage.flushSession()) ++flush_failures;
        else { pending_flush = 0; last_flush_ms = millis(); }
    }
}
void loadPrevious(uint32_t previous_id) {
    char tail[1024];
    size_t length = 0;
    bool from_start = true;
    if (!goblinStorage().readTail(previous_id, tail, sizeof(tail), length, from_start)) return;
    const LogTail status = inspectLogTail(tail, length);
    const char* cursor = tail;
    if (!from_start) {
        const char* newline = std::strchr(tail, '\n');
        if (!newline) return;
        cursor = newline + 1;
    }
    while (cursor && *cursor) {
        const char* end = std::strchr(cursor, '\n');
        if (!end) break;
        char line[1024];
        const size_t span = static_cast<size_t>(end - cursor);
        if (span && span < sizeof(line)) {
            std::memcpy(line, cursor, span);
            line[span] = 0;
            EventRecord event;
            if (eventFromJsonLine(line, event)) history.push(event);
        }
        cursor = end + 1;
    }
    if (status == LogTail::Truncated) {
        EventRecord event;
        fill(event, EventType::PreviousLogTruncated, EventCategory::Storage,
             "previous session ended with an incomplete line");
        enqueue(event, true);
    }
}
void sampleMemory() {
    const uint32_t heap = ESP.getFreeHeap();
    const uint32_t psram = ESP.getFreePsram();
    if (heap < min_heap) min_heap = heap;
    if (psram < min_psram) min_psram = psram;
}
void pumpRadios() {
    if (pumping) return;
    pumping = true;
    goblinBleTick();
    goblinAirTick();
    pumping = false;
}
void setAction(const char* text) {
    std::strncpy(action_text, text, sizeof(action_text) - 1);
    action_text[sizeof(action_text) - 1] = 0;
}
}

void goblinEventsBegin() {
    loadSettings();
    StorageManager& storage = goblinStorage();
    const bool mounted = storage.begin();
    PreferenceSessionStore store;
    const uint32_t highest = mounted ? storage.highestSessionId() : 0;
    const SessionAssignment assigned = assignSessionId(store, highest);
    session_id = assigned.id ? assigned.id : 1;
    counter_persisted = assigned.persisted;
    session_origin_ms = millis();
    last_summary_ms = session_origin_ms;
    last_flush_ms = session_origin_ms;
    reset_code = static_cast<int>(esp_reset_reason());
    std::strncpy(firmware_text, ESP_GOBLIN_VERSION, sizeof(firmware_text) - 1);
    std::strncpy(board_text, GoblinBoard::ID, sizeof(board_text) - 1);
    std::strncpy(reset_text, resetReasonName(reset_code), sizeof(reset_text) - 1);
    facts.firmware = firmware_text;
    facts.board_id = board_text;
    facts.reset_reason = reset_text;
    facts.sd_card_type = storage.cardTypeName();
    facts.flash_size = ESP.getFlashChipSize();
    facts.psram_size = ESP.getPsramSize();
    facts.free_heap = ESP.getFreeHeap();
    facts.free_psram = ESP.getFreePsram();
    facts.sd_size = storage.totalBytes();
    facts.trusted_ap_count = static_cast<uint32_t>(goblinTrustedStore().count());
    facts.wifi_monitor_enabled = false;
    facts.ble_privacy_enabled = false;
    sampleMemory();
    if (!counter_persisted) Serial.println("[log] session counter was not saved");
    Serial.printf("[log] session %lu logging=%s\n", static_cast<unsigned long>(session_id), logging_on ? "on" : "off");
    if (mounted && highest && highest < session_id) loadPrevious(highest);
    EventRecord boot;
    fill(boot, EventType::Boot, EventCategory::System, "ESP Goblin booted");
    eventCopyText(boot.source, sizeof(boot.source), reset_text, boot.flags);
    enqueue(boot, true);
    EventRecord started;
    char summary[80];
    std::snprintf(summary, sizeof(summary), "session %lu reset %s%s", static_cast<unsigned long>(session_id), reset_text,
                  counter_persisted ? "" : " counter-unsaved");
    fill(started, EventType::SessionStart, EventCategory::System, summary);
    enqueue(started, true);
    if (resetReasonUnexpected(reset_code)) {
        EventRecord unexpected;
        fill(unexpected, EventType::UnexpectedRestart, EventCategory::System, reset_text);
        eventCopyText(unexpected.source, sizeof(unexpected.source), reset_text, unexpected.flags);
        enqueue(unexpected, true);
    }
    EventRecord storage_event;
    if (storage.ready()) fill(storage_event, EventType::SdMounted, EventCategory::Storage, storage.cardTypeName());
    else fill(storage_event, EventType::SdMountFailed, EventCategory::Storage,
              storage.state() == StorageState::NoCard ? "SD card not present" : "SD mount failed");
    enqueue(storage_event, true);
    if (mounted) {
        storage_is_low = storageIsLow(storage.freeBytes(), storage.totalBytes(), kSdFreeReserveBytes, kSdLowSpacePercent);
        if (storage_is_low) {
            low_space_warned = true;
            EventRecord low;
            fill(low, EventType::SdLowSpace, EventCategory::Storage, "free space is below the reserve");
            enqueue(low, true);
        }
    }
    drain(true);
    esp_register_shutdown_handler(goblinShutdownHook);
}
void goblinEventsTick() {
    if (session_closed || pumping) return;
    sampleMemory();
    if (ESP.getFreeHeap() < kLowHeapBytes) {
        EventRecord low;
        fill(low, EventType::LowMemoryWarning, EventCategory::System, "free heap is low");
        low.metric[0] = ESP.getFreeHeap();
        enqueue(low, true);
    }
    const bool enabled = goblinRadio().enabled();
    if (enabled != monitor_was) {
        EventRecord event;
        fill(event, enabled ? EventType::AirMonitorStarted : EventType::AirMonitorStopped, EventCategory::Wifi,
             enabled ? "passive air monitor started" : "passive air monitor stopped");
        enqueue(event, true);
        monitor_was = enabled;
    }
    const uint32_t failures = goblinRadio().failures();
    if (failures != last_radio_failures) {
        EventRecord event;
        char summary[80];
        std::snprintf(summary, sizeof(summary), "radio transition failures %lu", static_cast<unsigned long>(failures));
        fill(event, EventType::RadioError, EventCategory::System, summary);
        event.metric[0] = failures;
        enqueue(event, true);
        last_radio_failures = failures;
    }
    const uint32_t now = millis();
    if (summaries_on && now - last_summary_ms >= kSummaryIntervalMs) {
        const uint32_t span = now - last_summary_ms;
        last_summary_ms = now;
        const AirCounters total = goblinAirMonitor().lifetime();
        static AirCounters previous;
        EventRecord air;
        air.metric[0] = static_cast<uint32_t>(total.management - previous.management);
        air.metric[1] = static_cast<uint32_t>(total.beacons - previous.beacons);
        air.metric[2] = static_cast<uint32_t>(total.probe_requests - previous.probe_requests);
        air.metric[3] = static_cast<uint32_t>(total.probe_responses - previous.probe_responses);
        air.metric[4] = static_cast<uint32_t>(total.deauthentication - previous.deauthentication);
        air.metric[5] = static_cast<uint32_t>(total.disassociation - previous.disassociation);
        air.metric[6] = static_cast<uint32_t>(total.queue_drops - previous.queue_drops);
        uint32_t top = 0;
        uint64_t top_count = 0;
        for (int channel = 1; channel <= 14; ++channel) {
            const uint64_t delta = total.channel[channel] - previous.channel[channel];
            if (delta >= top_count) { top_count = delta; top = static_cast<uint32_t>(channel); }
        }
        air.metric[7] = top;
        air.span_ms = span;
        previous = total;
        char summary[80];
        std::snprintf(summary, sizeof(summary), "mgmt %lu deauth %lu drops %lu ch %lu",
                      static_cast<unsigned long>(air.metric[0]), static_cast<unsigned long>(air.metric[4]),
                      static_cast<unsigned long>(air.metric[6]), static_cast<unsigned long>(top));
        fill(air, EventType::AirSummary, EventCategory::Wifi, summary);
        enqueue(air, true);
        uint32_t trackers = 0;
        uint32_t persistent = 0;
        for (size_t i = 0; i < goblinBleInventory().count(); ++i) {
            const BleDeviceRecord* record = goblinBleInventory().at(i);
            if (!record) continue;
            if (record->advertisement.classification == BleDeviceClass::GoogleFindHub ||
                record->advertisement.classification == BleDeviceClass::TrackerCandidate) ++trackers;
            if (record->persistent) ++persistent;
        }
        static uint64_t previous_observations = 0;
        static uint32_t previous_drops = 0;
        const uint64_t observations = goblinBleObservations();
        const uint32_t drops = goblinBleQueueDrops();
        EventRecord ble;
        ble.metric[0] = static_cast<uint32_t>(observations - previous_observations);
        ble.metric[1] = static_cast<uint32_t>(goblinBleInventory().count());
        ble.metric[2] = trackers;
        ble.metric[3] = persistent;
        ble.metric[4] = drops - previous_drops;
        ble.span_ms = span;
        previous_observations = observations;
        previous_drops = drops;
        std::snprintf(summary, sizeof(summary), "obs %lu devices %lu tracker %lu persistent %lu",
                      static_cast<unsigned long>(ble.metric[0]), static_cast<unsigned long>(ble.metric[1]),
                      static_cast<unsigned long>(trackers), static_cast<unsigned long>(persistent));
        fill(ble, EventType::BleSummary, EventCategory::Privacy, summary);
        enqueue(ble, true);
    }
    StorageManager& storage = goblinStorage();
    if (storage.ready()) {
        const bool low = storageIsLow(storage.freeBytes(), storage.totalBytes(), kSdFreeReserveBytes, kSdLowSpacePercent);
        storage_is_low = low;
        if (low && !low_space_warned) {
            low_space_warned = true;
            EventRecord event;
            fill(event, EventType::SdLowSpace, EventCategory::Storage, "free space is below the reserve");
            enqueue(event, true);
        } else if (retention_on) {
            static uint32_t last_retention_ms = 0;
            if (now - last_retention_ms >= 30000) {
                last_retention_ms = now;
                for (int pass = 0; pass < 4; ++pass) {
                    uint32_t removed = 0;
                    if (!storage.removeOldestSession(session_id, kMaxSessionFiles, low_space_warned, removed)) break;
                }
            }
        }
    }
    drain(false);
}
void goblinShutdownHook() { goblinEventsShutdown(); }
void goblinEventsShutdown() {
    if (session_closed) return;
    EventRecord event;
    fill(event, EventType::SessionEnd, EventCategory::System, "session closed");
    enqueue(event, false);
    const bool saved = logging_on;
    logging_on = true;
    drain(true);
    logging_on = saved;
    goblinStorage().closeSession();
    session_closed = true;
}
void goblinLogGuardAlerts() {
    const GuardAnalyzer& alerts = goblinGuardAlerts();
    for (size_t i = 0; i < alerts.count(); ++i) {
        const GuardAlert* alert = alerts.at(i);
        if (!alert || alert->dismissed || guardBleAlert(alert->type) || alert->type == GuardAlertType::DisconnectBurst) continue;
        EventType type = EventType::NewAccessPoint;
        if (alert->type == GuardAlertType::SsidCollision) type = EventType::SsidCollision;
        else if (alert->type == GuardAlertType::TrustedSsidChanged) type = EventType::TrustedSsidChanged;
        else if (alert->type == GuardAlertType::TrustedSecurityChanged) type = EventType::TrustedSecurityChanged;
        else if (alert->type == GuardAlertType::TrustedChannelChanged) type = EventType::TrustedChannelChanged;
        else if (alert->type != GuardAlertType::NewAccessPoint) continue;
        EventRecord event;
        fill(event, type, EventCategory::Guard, eventShortName(type));
        eventCopyText(event.ssid, sizeof(event.ssid), alert->wifi.ssid, event.flags);
        char address[18];
        guardFormatBssid(alert->wifi.bssid, address, sizeof(address));
        eventCopyText(event.bssid, sizeof(event.bssid), address, event.flags);
        if (alert->wifi.trusted_bssid[0] || alert->wifi.trusted_bssid[1] || alert->wifi.trusted_bssid[2] ||
            alert->wifi.trusted_bssid[3] || alert->wifi.trusted_bssid[4] || alert->wifi.trusted_bssid[5]) {
            guardFormatBssid(alert->wifi.trusted_bssid, address, sizeof(address));
            eventCopyText(event.trusted_bssid, sizeof(event.trusted_bssid), address, event.flags);
        }
        eventCopyText(event.old_value, sizeof(event.old_value), alert->wifi.old_value, event.flags);
        eventCopyText(event.new_value, sizeof(event.new_value), alert->wifi.new_value, event.flags);
        event.rssi = static_cast<int16_t>(alert->wifi.rssi);
        event.channel = static_cast<int16_t>(alert->wifi.channel);
        event.flags = static_cast<uint16_t>(event.flags | kFlagRssi | kFlagChannel);
        enqueue(event, true);
    }
}
void goblinLogDisconnect(const DisconnectBurst& burst, uint8_t subtype) {
    EventRecord event;
    const EventType type = subtype == 10 ? EventType::DisassocBurst : EventType::DeauthBurst;
    fill(event, type, EventCategory::Wifi, eventShortName(type));
    char address[18];
    guardFormatBssid(burst.bssid, address, sizeof(address));
    eventCopyText(event.bssid, sizeof(event.bssid), address, event.flags);
    guardFormatBssid(burst.source, address, sizeof(address));
    eventCopyText(event.source, sizeof(event.source), address, event.flags);
    event.rssi = burst.rssi;
    event.channel = burst.channel;
    event.count = burst.frame_count;
    event.span_ms = burst.window_ms;
    event.flags = static_cast<uint16_t>(event.flags | kFlagRssi | kFlagChannel | kFlagCount | kFlagTrusted);
    if (isTrustedAccessPoint(burst.bssid)) event.flags = static_cast<uint16_t>(event.flags | kFlagTrustedYes);
    if (burst.count_lower_bound) event.flags = static_cast<uint16_t>(event.flags | kFlagLowerBound);
    if (burst.has_reason_code) {
        event.reason_code = static_cast<int16_t>(burst.reason_code);
        event.flags = static_cast<uint16_t>(event.flags | kFlagReason);
    }
    enqueue(event, true);
}
void goblinLogBleAlert(const BleDeviceRecord& record, bool persistent) {
    EventRecord event;
    const EventType type = persistent ? EventType::BlePersistentTrackerCandidate : EventType::BleTrackerCapableObserved;
    fill(event, type, EventCategory::Privacy, eventShortName(type));
    char address[18];
    guardFormatBssid(record.key.address, address, sizeof(address));
    eventCopyText(event.ble_address, sizeof(event.ble_address), address, event.flags);
    eventCopyText(event.ble_name, sizeof(event.ble_name), record.advertisement.name, event.flags);
    eventCopyText(event.source, sizeof(event.source), bleClassName(record.advertisement.classification), event.flags);
    event.rssi = record.rssi;
    event.flags = static_cast<uint16_t>(event.flags | kFlagRssi);
    event.count = record.seen_count;
    event.flags = static_cast<uint16_t>(event.flags | kFlagCount);
    enqueue(event, true);
}
void goblinLogDismiss(const GuardAlert& alert) {
    EventRecord event;
    if (guardBleAlert(alert.type)) {
        fill(event, EventType::BlePrivacyAlertDismissed, EventCategory::Privacy, "privacy alert dismissed");
        char address[18];
        guardFormatBssid(alert.ble.key.address, address, sizeof(address));
        eventCopyText(event.ble_address, sizeof(event.ble_address), address, event.flags);
    } else {
        fill(event, EventType::AlertDismissed, EventCategory::User, guardAlertName(alert.type));
        char address[18];
        guardFormatBssid(alert.wifi.bssid, address, sizeof(address));
        eventCopyText(event.bssid, sizeof(event.bssid), address, event.flags);
        eventCopyText(event.ssid, sizeof(event.ssid), alert.wifi.ssid, event.flags);
    }
    enqueue(event, true);
}
void goblinLogTrust(bool added, const uint8_t* bssid, const char* ssid) {
    if (!added || !bssid) return;
    char address[18];
    guardFormatBssid(bssid, address, sizeof(address));
    EventRecord user;
    fill(user, EventType::UserMarkedApTrusted, EventCategory::User, "user marked AP trusted");
    eventCopyText(user.bssid, sizeof(user.bssid), address, user.flags);
    eventCopyText(user.ssid, sizeof(user.ssid), ssid, user.flags);
    enqueue(user, true);
    EventRecord guard;
    fill(guard, EventType::TrustedApAdded, EventCategory::Guard, "trusted AP added");
    eventCopyText(guard.bssid, sizeof(guard.bssid), address, guard.flags);
    eventCopyText(guard.ssid, sizeof(guard.ssid), ssid, guard.flags);
    enqueue(guard, true);
}
void goblinLogForget(const uint8_t* bssid, const char* ssid) {
    if (!bssid) return;
    char address[18];
    guardFormatBssid(bssid, address, sizeof(address));
    EventRecord user;
    fill(user, EventType::UserForgotAp, EventCategory::User, "user forgot AP");
    eventCopyText(user.bssid, sizeof(user.bssid), address, user.flags);
    eventCopyText(user.ssid, sizeof(user.ssid), ssid, user.flags);
    enqueue(user, true);
    EventRecord guard;
    fill(guard, EventType::TrustedApRemoved, EventCategory::Guard, "trusted AP removed");
    eventCopyText(guard.bssid, sizeof(guard.bssid), address, guard.flags);
    eventCopyText(guard.ssid, sizeof(guard.ssid), ssid, guard.flags);
    enqueue(guard, true);
}
void goblinLogConfig(const char* summary) {
    EventRecord event;
    fill(event, EventType::ConfigurationChanged, EventCategory::Configuration, summary ? summary : "configuration changed");
    enqueue(event, true);
}
const char* goblinStorageLabel() { return storageUiLabel(goblinStorage().state(), storage_is_low); }
uint32_t goblinSessionId() { return session_id; }
uint32_t goblinEventsWritten() { return events_written; }
uint32_t goblinEventsDropped() { return logger.dropped(); }
uint32_t goblinLoggerHighWater() { return logger.highWater(); }
uint32_t goblinLoggerQueued() { return logger.queued(); }
uint64_t goblinSdTotalBytes() { return goblinStorage().totalBytes(); }
uint64_t goblinSdFreeBytes() { return goblinStorage().freeBytes(); }
const char* goblinSdCardName() { return goblinStorage().cardTypeName(); }
bool goblinStorageReady() { return goblinStorage().ready(); }
bool goblinStorageLow() { return storage_is_low; }
bool goblinLoggingEnabled() { return logging_on; }
bool goblinSummariesEnabled() { return summaries_on; }
bool goblinChecksumEnabled() { return checksum_on; }
bool goblinRetentionEnabled() { return retention_on; }
uint32_t goblinSummaryIntervalMs() { return kSummaryIntervalMs; }
size_t goblinHistoryCount() { return history.count(); }
const EventRecord* goblinHistoryNewest(size_t offset) { return history.newest(offset); }
uint32_t goblinSessionOriginMs() { return session_origin_ms; }
const char* goblinLastFileAction() { return action_text; }
void goblinToggleLogging() {
    if (logging_on) {
        goblinLogConfig("event logging off");
        drain(true);
        logging_on = false;
    } else {
        logging_on = true;
        goblinLogConfig("event logging on");
    }
    saveSettings();
}
void goblinToggleSummaries() { summaries_on = !summaries_on; goblinLogConfig(summaries_on ? "summaries on" : "summaries off"); saveSettings(); }
void goblinToggleChecksum() { checksum_on = !checksum_on; goblinLogConfig(checksum_on ? "integrity checksum on" : "integrity checksum off"); saveSettings(); }
void goblinToggleRetention() { retention_on = !retention_on; goblinLogConfig(retention_on ? "retention automatic" : "retention off"); saveSettings(); }
bool goblinRetryStorage() {
    write_failed_noted = false;
    StorageManager& storage = goblinStorage();
    if (!storage.begin()) {
        EventRecord event;
        fill(event, EventType::SdMountFailed, EventCategory::Storage, "SD retry failed");
        enqueue(event, true);
        setAction("SD retry failed");
        return false;
    }
    low_space_warned = false;
    EventRecord mounted;
    fill(mounted, EventType::SdMounted, EventCategory::Storage, storage.cardTypeName());
    enqueue(mounted, true);
    EventRecord recovery;
    fill(recovery, EventType::LogRecovery, EventCategory::Storage, "logging resumed");
    enqueue(recovery, true);
    drain(true);
    setAction("SD recovered");
    return true;
}
bool goblinExportSession() {
    StorageManager& storage = goblinStorage();
    if (!storage.ready()) { setAction("Export failed: no SD"); return false; }
    drain(true);
    storage.closeSession();
    char json_file[48], csv_file[48], json_path[80], csv_path[80];
    if (!buildExportFileName(session_id, "jsonl", json_file, sizeof(json_file)) ||
        !buildExportFileName(session_id, "csv", csv_file, sizeof(csv_file)) ||
        !buildGoblinFilePath(json_path, sizeof(json_path), "export", json_file) ||
        !buildGoblinFilePath(csv_path, sizeof(csv_path), "export", csv_file)) {
        storage.openSession(session_id);
        setAction("Export path rejected");
        return false;
    }
    uint32_t skipped = 0;
    const bool copied = storage.copySession(session_id, json_path, pumpRadios);
    const bool csv = copied && storage.writeCsvExport(session_id, csv_path, skipped, pumpRadios);
    bool hashed = true;
    if (csv && checksum_on) {
        uint8_t hash[32];
        uint64_t size = 0;
        char sidecar_file[40], sidecar_path[80], text[160];
        char session_file_name[40];
        hashed = storage.hashSession(session_id, hash, size) &&
                 buildSessionFileName(session_id, session_file_name, sizeof(session_file_name)) &&
                 buildSidecarFileName(session_id, sidecar_file, sizeof(sidecar_file)) &&
                 buildGoblinFilePath(sidecar_path, sizeof(sidecar_path), "sessions", sidecar_file) &&
                 formatIntegritySidecar(session_file_name, hash, size, text, sizeof(text)) &&
                 storage.writeWholeFile(sidecar_path, text, std::strlen(text));
    }
    storage.openSession(session_id);
    if (!copied || !csv || !hashed) { setAction("Export incomplete"); return false; }
    std::snprintf(action_text, sizeof(action_text), skipped ? "Export wrote, skipped %lu" : "Export written",
                  static_cast<unsigned long>(skipped));
    return true;
}
bool goblinWriteDiagnostics() {
    DiagnosticSnapshot snapshot = {};
    snapshot.firmware = firmware_text;
    snapshot.board = board_text;
    snapshot.reset_reason = reset_text;
    snapshot.sd_state = storageStateName(goblinStorage().state());
    snapshot.sd_card = goblinStorage().cardTypeName();
    snapshot.session_id = session_id;
    snapshot.flash_size = ESP.getFlashChipSize();
    snapshot.psram_size = ESP.getPsramSize();
    snapshot.free_heap = ESP.getFreeHeap();
    snapshot.free_psram = ESP.getFreePsram();
    snapshot.min_heap = min_heap == UINT32_MAX ? 0 : min_heap;
    snapshot.min_psram = min_psram == UINT32_MAX ? 0 : min_psram;
    snapshot.sd_total = goblinStorage().totalBytes();
    snapshot.sd_free = goblinStorage().freeBytes();
    snapshot.mount_attempts = goblinStorage().mountAttempts();
    snapshot.mount_failures = goblinStorage().mountFailures();
    snapshot.write_failures = write_failures;
    snapshot.flush_failures = flush_failures;
    snapshot.events_written = events_written;
    snapshot.events_dropped = logger.dropped();
    snapshot.queue_capacity = static_cast<uint32_t>(kEventQueueCapacity);
    snapshot.queue_high_water = logger.highWater();
    snapshot.wifi_queue_drops = goblinWifiMonitor().queue().dropped();
    snapshot.ble_queue_drops = goblinBleQueueDrops();
    snapshot.radio_failures = goblinRadio().failures();
    snapshot.logging_enabled = logging_on;
    snapshot.summaries_enabled = summaries_on;
    snapshot.retention_enabled = retention_on;
    snapshot.checksum_enabled = checksum_on;
    char text[2048];
    const size_t length = formatDiagnostics(snapshot, text, sizeof(text));
    char path[64];
    if (!length || !buildGoblinFilePath(path, sizeof(path), "diagnostics", "diagnostics.txt") ||
        !goblinStorage().writeWholeFile(path, text, length)) {
        setAction("Diagnostics failed");
        return false;
    }
    setAction("Diagnostics written");
    return true;
}
