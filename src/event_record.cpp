#include "event_record.h"
#include <cstdio>
#include <cstring>

const char* eventCategoryName(EventCategory category) {
    switch (category) {
        case EventCategory::System: return "system";
        case EventCategory::Storage: return "storage";
        case EventCategory::Guard: return "guard";
        case EventCategory::Wifi: return "wifi";
        case EventCategory::Ble: return "ble";
        case EventCategory::Privacy: return "privacy";
        case EventCategory::User: return "user";
        case EventCategory::Configuration: return "configuration";
    }
    return "system";
}
const char* eventTypeName(EventType type) {
    switch (type) {
        case EventType::Boot: return "BOOT";
        case EventType::SessionStart: return "SESSION_START";
        case EventType::SessionEnd: return "SESSION_END";
        case EventType::TimeSync: return "TIME_SYNC";
        case EventType::LowMemoryWarning: return "LOW_MEMORY_WARNING";
        case EventType::UnexpectedRestart: return "UNEXPECTED_RESTART";
        case EventType::PreviousLogTruncated: return "PREVIOUS_LOG_TRUNCATED";
        case EventType::SdMounted: return "SD_MOUNTED";
        case EventType::SdRemoved: return "SD_REMOVED";
        case EventType::SdMountFailed: return "SD_MOUNT_FAILED";
        case EventType::SdWriteFailed: return "SD_WRITE_FAILED";
        case EventType::SdLowSpace: return "SD_LOW_SPACE";
        case EventType::LogQueueOverflow: return "LOG_QUEUE_OVERFLOW";
        case EventType::LogRecovery: return "LOG_RECOVERY";
        case EventType::NewAccessPoint: return "NEW_ACCESS_POINT";
        case EventType::TrustedApAdded: return "TRUSTED_AP_ADDED";
        case EventType::TrustedApRemoved: return "TRUSTED_AP_REMOVED";
        case EventType::TrustedSsidChanged: return "TRUSTED_SSID_CHANGED";
        case EventType::TrustedSecurityChanged: return "TRUSTED_SECURITY_CHANGED";
        case EventType::TrustedChannelChanged: return "TRUSTED_CHANNEL_CHANGED";
        case EventType::SsidCollision: return "SSID_COLLISION";
        case EventType::DeauthBurst: return "DEAUTH_BURST";
        case EventType::DisassocBurst: return "DISASSOC_BURST";
        case EventType::AirMonitorStarted: return "AIR_MONITOR_STARTED";
        case EventType::AirMonitorStopped: return "AIR_MONITOR_STOPPED";
        case EventType::AirSummary: return "AIR_SUMMARY";
        case EventType::BleTrackerCapableObserved: return "BLE_TRACKER_CAPABLE_OBSERVED";
        case EventType::BlePersistentTrackerCandidate: return "BLE_PERSISTENT_TRACKER_CANDIDATE";
        case EventType::BlePrivacyAlertDismissed: return "BLE_PRIVACY_ALERT_DISMISSED";
        case EventType::BleSummary: return "BLE_SUMMARY";
        case EventType::AlertDismissed: return "ALERT_DISMISSED";
        case EventType::UserMarkedApTrusted: return "USER_MARKED_AP_TRUSTED";
        case EventType::UserForgotAp: return "USER_FORGOT_AP";
        case EventType::ConfigurationChanged: return "CONFIGURATION_CHANGED";
        case EventType::RadioError: return "RADIO_ERROR";
    }
    return "BOOT";
}
const char* eventShortName(EventType type) {
    switch (type) {
        case EventType::SessionStart: return "SESSION START";
        case EventType::SessionEnd: return "SESSION END";
        case EventType::TimeSync: return "TIME SYNC";
        case EventType::LowMemoryWarning: return "LOW MEMORY";
        case EventType::UnexpectedRestart: return "UNEXPECTED RESTART";
        case EventType::PreviousLogTruncated: return "LOG TRUNCATED";
        case EventType::SdMounted: return "SD MOUNTED";
        case EventType::SdRemoved: return "SD REMOVED";
        case EventType::SdMountFailed: return "SD MOUNT FAILED";
        case EventType::SdWriteFailed: return "STORAGE ERROR";
        case EventType::SdLowSpace: return "SD LOW";
        case EventType::LogQueueOverflow: return "LOG OVERFLOW";
        case EventType::LogRecovery: return "LOG RECOVERY";
        case EventType::NewAccessPoint: return "NEW AP";
        case EventType::TrustedApAdded: return "TRUSTED AP";
        case EventType::TrustedApRemoved: return "AP REMOVED";
        case EventType::TrustedSsidChanged: return "SSID CHANGED";
        case EventType::TrustedSecurityChanged: return "SECURITY CHANGED";
        case EventType::TrustedChannelChanged: return "CHANNEL CHANGED";
        case EventType::SsidCollision: return "SSID COLLISION";
        case EventType::DeauthBurst: return "DEAUTH BURST";
        case EventType::DisassocBurst: return "DISASSOC BURST";
        case EventType::AirMonitorStarted: return "AIR STARTED";
        case EventType::AirMonitorStopped: return "AIR STOPPED";
        case EventType::AirSummary: return "AIR SUMMARY";
        case EventType::BleTrackerCapableObserved: return "TRACKER-CAPABLE";
        case EventType::BlePersistentTrackerCandidate: return "PERSISTENT TRACKER";
        case EventType::BlePrivacyAlertDismissed: return "PRIVACY DISMISSED";
        case EventType::BleSummary: return "BLE SUMMARY";
        case EventType::AlertDismissed: return "ALERT DISMISSED";
        case EventType::UserMarkedApTrusted: return "MARKED TRUSTED";
        case EventType::UserForgotAp: return "FORGOT AP";
        case EventType::ConfigurationChanged: return "CONFIG CHANGED";
        case EventType::RadioError: return "RADIO ERROR";
        case EventType::Boot: return "BOOT";
    }
    return "EVENT";
}
const char* eventSeverityName(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::Notice: return "notice";
        case EventSeverity::Warning: return "warning";
        case EventSeverity::Critical: return "critical";
        case EventSeverity::Info: return "info";
    }
    return "info";
}
EventSeverity eventDefaultSeverity(EventType type) {
    switch (type) {
        case EventType::UnexpectedRestart:
        case EventType::SdWriteFailed:
            return EventSeverity::Critical;
        case EventType::LowMemoryWarning:
        case EventType::SdMountFailed:
        case EventType::SdRemoved:
        case EventType::SdLowSpace:
        case EventType::LogQueueOverflow:
        case EventType::SsidCollision:
        case EventType::TrustedSecurityChanged:
        case EventType::DeauthBurst:
        case EventType::DisassocBurst:
        case EventType::BlePersistentTrackerCandidate:
        case EventType::RadioError:
        case EventType::PreviousLogTruncated:
            return EventSeverity::Warning;
        case EventType::NewAccessPoint:
        case EventType::TrustedSsidChanged:
        case EventType::TrustedChannelChanged:
        case EventType::BleTrackerCapableObserved:
        case EventType::SdMounted:
        case EventType::LogRecovery:
            return EventSeverity::Notice;
        default:
            return EventSeverity::Info;
    }
}
DedupMode eventDedupMode(EventType type) {
    switch (type) {
        case EventType::NewAccessPoint:
        case EventType::SsidCollision:
        case EventType::TrustedSsidChanged:
        case EventType::TrustedSecurityChanged:
        case EventType::TrustedChannelChanged:
        case EventType::BleTrackerCapableObserved:
        case EventType::BlePersistentTrackerCandidate:
            return DedupMode::Once;
        case EventType::SdLowSpace:
        case EventType::LowMemoryWarning:
        case EventType::LogQueueOverflow:
        case EventType::RadioError:
        case EventType::SdWriteFailed:
            return DedupMode::Cooldown;
        default:
            return DedupMode::None;
    }
}
void eventCopyText(char* dest, size_t capacity, const char* source, uint16_t& flags) {
    if (!dest || !capacity) return;
    if (!source) { dest[0] = 0; return; }
    size_t index = 0;
    while (source[index] && index + 1 < capacity) {
        dest[index] = source[index];
        ++index;
    }
    dest[index] = 0;
    if (source[index]) flags = static_cast<uint16_t>(flags | kFlagTruncated);
}
void eventDedupIdentity(const EventRecord& event, char* out, size_t capacity) {
    if (!out || !capacity) return;
    std::snprintf(out, capacity, "%s|%s|%s|%s|%s|%s",
                  event.bssid, event.trusted_bssid, event.ble_address, event.ssid,
                  event.old_value, event.new_value);
}

namespace {
class JsonOut {
public:
    JsonOut(char* out, size_t capacity) : out_(out), capacity_(capacity), used_(0), ok_(out && capacity > 2) {}
    bool ok() const { return ok_; }
    size_t used() const { return used_; }
    void begin() { add('{'); }
    bool end() {
        add('}');
        add('\n');
        if (!ok_ || used_ >= capacity_) { fail(); return false; }
        out_[used_] = 0;
        return true;
    }
    void key(const char* name) {
        if (used_ > 1) add(',');
        add('"');
        raw(name);
        add('"');
        add(':');
    }
    void number(const char* name, uint64_t value, bool negative) {
        key(name);
        if (negative) add('-');
        char digits[24];
        size_t length = 0;
        uint64_t rest = value;
        do { digits[length++] = static_cast<char>('0' + rest % 10); rest /= 10; } while (rest && length < sizeof(digits));
        while (length && ok_) add(digits[--length]);
    }
    void boolean(const char* name, bool value) { key(name); raw(value ? "true" : "false"); }
    void text(const char* name, const char* value) {
        key(name);
        add('"');
        if (!value) value = "";
        for (size_t i = 0; value[i] && ok_; ++i) escape(static_cast<unsigned char>(value[i]));
        add('"');
    }
private:
    void fail() { ok_ = false; if (out_ && capacity_) out_[0] = 0; used_ = 0; }
    void add(char value) {
        if (!ok_ || used_ + 1 >= capacity_) { fail(); return; }
        out_[used_++] = value;
    }
    void raw(const char* value) { for (size_t i = 0; value[i] && ok_; ++i) add(value[i]); }
    void escape(unsigned char value) {
        if (value == '"' || value == '\\') { add('\\'); add(static_cast<char>(value)); return; }
        if (value >= 32 && value != 127) { add(static_cast<char>(value)); return; }
        char hex[7];
        std::snprintf(hex, sizeof(hex), "\\u%04X", value);
        raw(hex);
    }
    char* out_;
    size_t capacity_;
    size_t used_;
    bool ok_;
};
bool airType(EventType type) { return type == EventType::AirSummary; }
bool bleType(EventType type) { return type == EventType::BleSummary; }
}

bool eventToJsonLine(const EventRecord& event, const SessionFacts* facts, char* out, size_t capacity, size_t& length) {
    length = 0;
    JsonOut json(out, capacity);
    json.begin();
    json.number("schema", event.schema, false);
    json.number("session", event.session_id, false);
    json.number("event", event.event_id, false);
    json.number("mono_ms", event.monotonic_ms, false);
    json.boolean("time_valid", (event.flags & kFlagTimeValid) != 0);
    if (event.flags & kFlagTimeValid) json.number("unix_ms", event.unix_ms, false);
    json.text("category", eventCategoryName(event.category));
    json.text("type", eventTypeName(event.type));
    json.text("severity", eventSeverityName(event.severity));
    json.text("summary", event.summary);
    if (event.flags & kFlagTruncated) json.boolean("truncated", true);
    if (event.ssid[0]) json.text("ssid", event.ssid);
    if (event.bssid[0]) json.text("bssid", event.bssid);
    if (event.trusted_bssid[0]) json.text("trusted_bssid", event.trusted_bssid);
    if (event.ble_address[0]) json.text("ble_address", event.ble_address);
    if (event.ble_name[0]) json.text("ble_name", event.ble_name);
    if (event.old_value[0]) json.text("old_value", event.old_value);
    if (event.new_value[0]) json.text("new_value", event.new_value);
    if (event.source[0]) json.text("source", event.source);
    if (event.flags & kFlagRssi) json.number("rssi", static_cast<uint64_t>(event.rssi < 0 ? -event.rssi : event.rssi), event.rssi < 0);
    if (event.flags & kFlagChannel) json.number("channel", static_cast<uint16_t>(event.channel), event.channel < 0);
    if (event.flags & kFlagCount) json.number("frames", event.count, false);
    if (event.span_ms) json.number("window_ms", event.span_ms, false);
    if (event.flags & kFlagTrusted) json.boolean("trusted", (event.flags & kFlagTrustedYes) != 0);
    if (event.flags & kFlagLowerBound) json.boolean("count_lower_bound", true);
    if (event.flags & kFlagReason) json.number("reason_code", static_cast<uint16_t>(event.reason_code), false);
    if (airType(event.type)) {
        json.number("management_frames", event.metric[0], false);
        json.number("beacons", event.metric[1], false);
        json.number("probe_requests", event.metric[2], false);
        json.number("probe_responses", event.metric[3], false);
        json.number("deauth", event.metric[4], false);
        json.number("disassoc", event.metric[5], false);
        json.number("queue_drops", event.metric[6], false);
        json.number("top_channel", event.metric[7], false);
    }
    if (bleType(event.type)) {
        json.number("observations", event.metric[0], false);
        json.number("unique_runtime_devices", event.metric[1], false);
        json.number("tracker_capable", event.metric[2], false);
        json.number("persistent_candidates", event.metric[3], false);
        json.number("queue_drops", event.metric[4], false);
    }
    if (event.type == EventType::SessionStart && facts) {
        json.text("firmware_version", facts->firmware ? facts->firmware : "");
        json.text("board_id", facts->board_id ? facts->board_id : "");
        json.text("reset_reason", facts->reset_reason ? facts->reset_reason : "unknown");
        json.number("flash_size", facts->flash_size, false);
        json.number("psram_size", facts->psram_size, false);
        json.number("free_heap", facts->free_heap, false);
        json.number("free_psram", facts->free_psram, false);
        json.text("sd_card_type", facts->sd_card_type ? facts->sd_card_type : "none");
        json.number("sd_size", facts->sd_size, false);
        json.number("trusted_ap_count", facts->trusted_ap_count, false);
        json.boolean("wifi_monitor_enabled", facts->wifi_monitor_enabled);
        json.boolean("ble_privacy_enabled", facts->ble_privacy_enabled);
    }
    if (!json.end()) return false;
    length = json.used();
    return true;
}

namespace {
const char* skip(const char* cursor) {
    while (cursor && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')) ++cursor;
    return cursor;
}
bool take(const char*& cursor, char expected) {
    cursor = skip(cursor);
    if (!cursor || *cursor != expected) return false;
    ++cursor;
    return true;
}
bool parseString(const char*& cursor, char* dest, size_t capacity) {
    if (!take(cursor, '"')) return false;
    size_t used = 0;
    while (*cursor && *cursor != '"') {
        unsigned char value = static_cast<unsigned char>(*cursor++);
        if (value == '\\') {
            if (!*cursor) return false;
            char escaped = *cursor++;
            if (escaped == 'u') {
                unsigned code = 0;
                for (int nibble = 0; nibble < 4; ++nibble) {
                    char hex = *cursor++;
                    code <<= 4;
                    if (hex >= '0' && hex <= '9') code += static_cast<unsigned>(hex - '0');
                    else if (hex >= 'A' && hex <= 'F') code += static_cast<unsigned>(hex - 'A' + 10);
                    else if (hex >= 'a' && hex <= 'f') code += static_cast<unsigned>(hex - 'a' + 10);
                    else return false;
                }
                value = static_cast<unsigned char>(code & 0xFF);
            } else if (escaped == 'n') value = '\n';
            else if (escaped == 'r') value = '\r';
            else if (escaped == 't') value = '\t';
            else value = static_cast<unsigned char>(escaped);
        }
        if (dest && used + 1 < capacity) dest[used++] = static_cast<char>(value);
    }
    if (dest && capacity) dest[used] = 0;
    return take(cursor, '"');
}
bool parseUint(const char*& cursor, uint64_t& value, bool& negative) {
    cursor = skip(cursor);
    negative = *cursor == '-';
    if (*cursor == '-' || *cursor == '+') ++cursor;
    if (*cursor < '0' || *cursor > '9') return false;
    value = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10u + static_cast<uint64_t>(*cursor - '0');
        ++cursor;
    }
    return true;
}
EventType typeFromName(const char* name) {
    for (int value = 0; value <= static_cast<int>(EventType::RadioError); ++value) {
        EventType type = static_cast<EventType>(value);
        if (!std::strcmp(name, eventTypeName(type))) return type;
    }
    return EventType::Boot;
}
EventCategory categoryFromName(const char* name) {
    for (int value = 0; value <= static_cast<int>(EventCategory::Configuration); ++value) {
        EventCategory category = static_cast<EventCategory>(value);
        if (!std::strcmp(name, eventCategoryName(category))) return category;
    }
    return EventCategory::System;
}
EventSeverity severityFromName(const char* name) {
    if (!std::strcmp(name, "notice")) return EventSeverity::Notice;
    if (!std::strcmp(name, "warning")) return EventSeverity::Warning;
    if (!std::strcmp(name, "critical")) return EventSeverity::Critical;
    return EventSeverity::Info;
}
}

bool eventFromJsonLine(const char* line, EventRecord& event) {
    event = EventRecord();
    if (!line) return false;
    const char* cursor = line;
    if (!take(cursor, '{')) return false;
    bool saw_type = false;
    while (true) {
        cursor = skip(cursor);
        if (*cursor == '}') { ++cursor; break; }
        char key[40] = {};
        if (!parseString(cursor, key, sizeof(key)) || !take(cursor, ':')) return false;
        cursor = skip(cursor);
        char text[80] = {};
        uint64_t number = 0;
        bool negative = false;
        bool is_text = *cursor == '"';
        bool is_bool = *cursor == 't' || *cursor == 'f';
        bool flag = false;
        if (is_text) { if (!parseString(cursor, text, sizeof(text))) return false; }
        else if (is_bool) {
            if (!std::strncmp(cursor, "true", 4)) { flag = true; cursor += 4; }
            else if (!std::strncmp(cursor, "false", 5)) { flag = false; cursor += 5; }
            else return false;
        } else if (!parseUint(cursor, number, negative)) return false;
        if (!std::strcmp(key, "schema")) event.schema = static_cast<uint8_t>(number);
        else if (!std::strcmp(key, "session")) event.session_id = static_cast<uint32_t>(number);
        else if (!std::strcmp(key, "event")) event.event_id = static_cast<uint32_t>(number);
        else if (!std::strcmp(key, "mono_ms")) event.monotonic_ms = static_cast<uint32_t>(number);
        else if (!std::strcmp(key, "time_valid")) { if (flag) event.flags = static_cast<uint16_t>(event.flags | kFlagTimeValid); }
        else if (!std::strcmp(key, "unix_ms")) event.unix_ms = number;
        else if (!std::strcmp(key, "category")) event.category = categoryFromName(text);
        else if (!std::strcmp(key, "type")) { event.type = typeFromName(text); saw_type = true; }
        else if (!std::strcmp(key, "severity")) event.severity = severityFromName(text);
        else if (!std::strcmp(key, "summary")) eventCopyText(event.summary, sizeof(event.summary), text, event.flags);
        else if (!std::strcmp(key, "ssid")) eventCopyText(event.ssid, sizeof(event.ssid), text, event.flags);
        else if (!std::strcmp(key, "bssid")) eventCopyText(event.bssid, sizeof(event.bssid), text, event.flags);
        else if (!std::strcmp(key, "trusted_bssid")) eventCopyText(event.trusted_bssid, sizeof(event.trusted_bssid), text, event.flags);
        else if (!std::strcmp(key, "ble_address")) eventCopyText(event.ble_address, sizeof(event.ble_address), text, event.flags);
        else if (!std::strcmp(key, "ble_name")) eventCopyText(event.ble_name, sizeof(event.ble_name), text, event.flags);
        else if (!std::strcmp(key, "old_value")) eventCopyText(event.old_value, sizeof(event.old_value), text, event.flags);
        else if (!std::strcmp(key, "new_value")) eventCopyText(event.new_value, sizeof(event.new_value), text, event.flags);
        else if (!std::strcmp(key, "source")) eventCopyText(event.source, sizeof(event.source), text, event.flags);
        else if (!std::strcmp(key, "rssi")) { event.rssi = static_cast<int16_t>(negative ? -static_cast<int64_t>(number) : number); event.flags = static_cast<uint16_t>(event.flags | kFlagRssi); }
        else if (!std::strcmp(key, "channel")) { event.channel = static_cast<int16_t>(number); event.flags = static_cast<uint16_t>(event.flags | kFlagChannel); }
        else if (!std::strcmp(key, "frames")) { event.count = static_cast<uint32_t>(number); event.flags = static_cast<uint16_t>(event.flags | kFlagCount); }
        else if (!std::strcmp(key, "window_ms")) event.span_ms = static_cast<uint32_t>(number);
        else if (!std::strcmp(key, "trusted")) { event.flags = static_cast<uint16_t>(event.flags | kFlagTrusted); if (flag) event.flags = static_cast<uint16_t>(event.flags | kFlagTrustedYes); }
        else if (!std::strcmp(key, "truncated") && flag) event.flags = static_cast<uint16_t>(event.flags | kFlagTruncated);
        else if (!std::strcmp(key, "count_lower_bound") && flag) event.flags = static_cast<uint16_t>(event.flags | kFlagLowerBound);
        else if (!std::strcmp(key, "reason_code")) { event.reason_code = static_cast<int16_t>(number); event.flags = static_cast<uint16_t>(event.flags | kFlagReason); }
        cursor = skip(cursor);
        if (*cursor == ',') { ++cursor; continue; }
        if (*cursor == '}') { ++cursor; break; }
        return false;
    }
    cursor = skip(cursor);
    return saw_type && (*cursor == 0 || *cursor == '\n');
}
