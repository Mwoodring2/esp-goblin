#include "log_policy.h"
#include <cstdio>
#include <cstring>

const char* storageStateName(StorageState state) {
    switch (state) {
        case StorageState::Mounting: return "mounting";
        case StorageState::Ready: return "ready";
        case StorageState::NoCard: return "no_card";
        case StorageState::MountFailed: return "mount_failed";
        case StorageState::ReadOnly: return "read_only";
        case StorageState::Faulted: return "faulted";
        case StorageState::Uninitialized: return "uninitialized";
    }
    return "uninitialized";
}
const char* storageUiLabel(StorageState state, bool low_space) {
    if (state == StorageState::Ready && low_space) return "SD LOW";
    if (state == StorageState::Ready) return "SD OK";
    if (state == StorageState::Faulted || state == StorageState::MountFailed || state == StorageState::ReadOnly) return "SD ERR";
    return "SD --";
}
StorageState storageStateAfterMount(bool card_seen, bool mounted, bool writable) {
    if (!card_seen) return StorageState::NoCard;
    if (!mounted) return StorageState::MountFailed;
    if (!writable) return StorageState::ReadOnly;
    return StorageState::Ready;
}
StorageState storageStateAfterWrite(StorageState state, bool write_ok) {
    if (!write_ok && state == StorageState::Ready) return StorageState::Faulted;
    return state;
}
const char* resetReasonName(int code) {
    switch (code) {
        case 1: return "power_on";
        case 2: return "external";
        case 3: return "software";
        case 4: return "panic";
        case 5: return "interrupt_watchdog";
        case 6: return "task_watchdog";
        case 7: return "watchdog";
        case 8: return "deep_sleep";
        case 9: return "brownout";
        case 10: return "sdio";
        case 11: return "usb";
        case 12: return "jtag";
        default: return "unknown";
    }
}
bool resetReasonUnexpected(int code) {
    return code == 4 || code == 5 || code == 6 || code == 7 || code == 9;
}
bool wallClockPlausible(uint64_t unix_ms) {
    return unix_ms >= kWallClockMinUnixMs && unix_ms < kWallClockMaxUnixMs;
}
uint32_t nextSessionId(uint32_t stored_next, bool stored_ok, uint32_t highest_file_id) {
    const uint32_t from_store = (stored_ok && stored_next) ? stored_next : 1u;
    const uint32_t from_files = highest_file_id + 1u;
    return from_files > from_store ? from_files : from_store;
}
SessionAssignment assignSessionId(SessionIdStore& store, uint32_t highest_file_id) {
    SessionAssignment result;
    uint32_t stored = 1;
    const bool stored_ok = store.readNext(stored);
    result.id = nextSessionId(stored, stored_ok, highest_file_id);
    result.persisted = store.writeNext(result.id + 1u);
    return result;
}
uint64_t lowSpaceLimit(uint64_t total_bytes, uint64_t reserve_bytes, uint8_t percent) {
    if (!total_bytes || !percent) return reserve_bytes;
    const uint64_t percent_bytes = total_bytes * percent / 100u;
    if (!percent_bytes) return reserve_bytes;
    return percent_bytes < reserve_bytes ? percent_bytes : reserve_bytes;
}
bool storageIsLow(uint64_t free_bytes, uint64_t total_bytes, uint64_t reserve_bytes, uint8_t percent) {
    return free_bytes < lowSpaceLimit(total_bytes, reserve_bytes, percent);
}
bool retentionNeeded(uint32_t session_files, uint32_t max_files, uint64_t free_bytes,
                     uint64_t reserve_bytes, bool low_space_already_warned) {
    if (session_files > max_files) return true;
    return low_space_already_warned && free_bytes < reserve_bytes;
}
bool selectOldestSession(const uint32_t* ids, size_t count, uint32_t current, uint32_t& oldest) {
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
        if (!ids || ids[i] == current) continue;
        if (!found || ids[i] < oldest) { oldest = ids[i]; found = true; }
    }
    return found;
}
bool logFlushDue(uint32_t now_ms, uint32_t last_flush_ms, uint32_t pending, bool important, bool force) {
    if (force) return true;
    if (!pending) return false;
    if (important || pending >= kLogFlushEventCount) return true;
    return now_ms - last_flush_ms >= kLogFlushIntervalMs;
}
LogTail inspectLogTail(const char* data, size_t length) {
    if (!length || !data) return LogTail::Empty;
    return data[length - 1] == '\n' ? LogTail::Complete : LogTail::Truncated;
}
bool pathInsideGoblin(const char* path) {
    if (!path || path[0] != '/') return false;
    for (const char* cursor = path; *cursor; ++cursor) {
        if (*cursor == '\\') return false;
        if (cursor[0] == '.' && cursor[1] == '.') return false;
    }
    const char* root = std::strstr(path, "/goblin");
    if (!root) return false;
    const char next = root[7];
    return next == 0 || next == '/';
}
namespace {
bool singleSegment(const char* value) {
    if (!value || !value[0]) return false;
    return !std::strchr(value, '/') && !std::strchr(value, '\\') && !std::strstr(value, "..");
}
}
bool buildSessionFileName(uint32_t id, char* out, size_t capacity) {
    if (!out || !capacity) return false;
    const int wrote = std::snprintf(out, capacity, id <= 999999u ? "session_%06lu.jsonl" : "session_%lu.jsonl",
                                    static_cast<unsigned long>(id));
    uint32_t parsed = 0;
    return wrote > 0 && static_cast<size_t>(wrote) < capacity && parseSessionFileName(out, parsed) && parsed == id;
}
bool buildSidecarFileName(uint32_t id, char* out, size_t capacity) {
    if (!out || !capacity) return false;
    const int wrote = std::snprintf(out, capacity, id <= 999999u ? "session_%06lu.sha256" : "session_%lu.sha256",
                                    static_cast<unsigned long>(id));
    return wrote > 0 && static_cast<size_t>(wrote) < capacity && !std::strstr(out, "..");
}
bool buildExportFileName(uint32_t id, const char* extension, char* out, size_t capacity) {
    if (!singleSegment(extension) || std::strchr(extension, '.')) return false;
    if (!out || !capacity) return false;
    const int wrote = std::snprintf(out, capacity,
                                    id <= 999999u ? "goblin_export_session_%06lu.%s" : "goblin_export_session_%lu.%s",
                                    static_cast<unsigned long>(id), extension);
    return wrote > 0 && static_cast<size_t>(wrote) < capacity && singleSegment(out);
}
bool buildGoblinFilePath(char* out, size_t capacity, const char* folder, const char* file) {
    if (!singleSegment(folder) || !singleSegment(file) || !out || !capacity) return false;
    const int wrote = std::snprintf(out, capacity, "/goblin/%s/%s", folder, file);
    return wrote > 0 && static_cast<size_t>(wrote) < capacity && pathInsideGoblin(out);
}
bool parseSessionFileName(const char* name, uint32_t& id) {
    if (!name) return false;
    const char prefix[] = "session_";
    const char* cursor = name;
    for (size_t i = 0; prefix[i]; ++i) if (*cursor++ != prefix[i]) return false;
    if (*cursor < '0' || *cursor > '9') return false;
    uint32_t value = 0;
    int digits = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        if (++digits > 10) return false;
        value = value * 10u + static_cast<uint32_t>(*cursor++ - '0');
    }
    if (std::strcmp(cursor, ".jsonl")) return false;
    id = value;
    return true;
}
void formatEvidenceTime(bool time_valid, uint64_t unix_ms, uint32_t monotonic_ms, uint32_t origin_ms,
                        char* out, size_t capacity) {
    if (!out || !capacity) return;
    if (time_valid && wallClockPlausible(unix_ms)) {
        const uint64_t minutes = unix_ms / 1000u / 60u;
        std::snprintf(out, capacity, "%02u:%02u UTC",
                      static_cast<unsigned>((minutes / 60u) % 24u), static_cast<unsigned>(minutes % 60u));
        return;
    }
    const uint32_t elapsed = monotonic_ms >= origin_ms ? monotonic_ms - origin_ms : 0;
    const uint32_t total_seconds = elapsed / 1000u;
    const uint32_t hours = total_seconds / 3600u;
    const uint32_t minutes = (total_seconds % 3600u) / 60u;
    const uint32_t seconds = total_seconds % 60u;
    if (hours) std::snprintf(out, capacity, "+%luh%02lum", static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes));
    else std::snprintf(out, capacity, "+%02lum%02lus", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
}
void formatByteCount(uint64_t bytes, char* out, size_t capacity) {
    if (!out || !capacity) return;
    const uint64_t kib = 1024ull;
    const uint64_t mib = kib * 1024ull;
    const uint64_t gib = mib * 1024ull;
    if (bytes >= gib) {
        const uint64_t whole = bytes / gib;
        const uint64_t tenth = (bytes % gib) * 10u / gib;
        std::snprintf(out, capacity, "%llu.%llu GB", static_cast<unsigned long long>(whole), static_cast<unsigned long long>(tenth));
    } else if (bytes >= mib) {
        std::snprintf(out, capacity, "%llu MB", static_cast<unsigned long long>(bytes / mib));
    } else if (bytes >= kib) {
        std::snprintf(out, capacity, "%llu KB", static_cast<unsigned long long>(bytes / kib));
    } else std::snprintf(out, capacity, "%llu B", static_cast<unsigned long long>(bytes));
}
namespace {
bool csvAppend(char* out, size_t capacity, size_t& used, const char* text, bool quoted) {
    if (quoted && used + 1 < capacity) out[used++] = '"';
    for (size_t i = 0; text && text[i]; ++i) {
        if (text[i] == '"') {
            if (used + 2 >= capacity) return false;
            out[used++] = '"';
            out[used++] = '"';
        } else {
            if (used + 1 >= capacity) return false;
            out[used++] = text[i];
        }
    }
    if (quoted) { if (used + 1 >= capacity) return false; out[used++] = '"'; }
    return true;
}
bool csvComma(char* out, size_t capacity, size_t& used) {
    if (used + 1 >= capacity) return false;
    out[used++] = ',';
    return true;
}
bool needsQuotes(const char* text) {
    for (size_t i = 0; text && text[i]; ++i)
        if (text[i] == '"' || text[i] == ',' || text[i] == '\n' || text[i] == '\r') return true;
    return false;
}
}
const char* csvHeader() {
    return "session_id,event_id,timestamp,monotonic_ms,category,type,severity,summary,ssid,bssid,ble_address,rssi,channel\n";
}
bool csvRowFromEvent(const EventRecord& event, char* out, size_t capacity, size_t& length) {
    length = 0;
    if (!out || capacity < 8) return false;
    char number[32];
    size_t used = 0;
    std::snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(event.session_id));
    if (!csvAppend(out, capacity, used, number, false) || !csvComma(out, capacity, used)) return false;
    std::snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(event.event_id));
    if (!csvAppend(out, capacity, used, number, false) || !csvComma(out, capacity, used)) return false;
    if (event.flags & kFlagTimeValid) std::snprintf(number, sizeof(number), "%llu", static_cast<unsigned long long>(event.unix_ms));
    else number[0] = 0;
    if (!csvAppend(out, capacity, used, number, false) || !csvComma(out, capacity, used)) return false;
    std::snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(event.monotonic_ms));
    if (!csvAppend(out, capacity, used, number, false) || !csvComma(out, capacity, used)) return false;
    if (!csvAppend(out, capacity, used, eventCategoryName(event.category), false) || !csvComma(out, capacity, used)) return false;
    if (!csvAppend(out, capacity, used, eventTypeName(event.type), false) || !csvComma(out, capacity, used)) return false;
    if (!csvAppend(out, capacity, used, eventSeverityName(event.severity), false) || !csvComma(out, capacity, used)) return false;
    if (!csvAppend(out, capacity, used, event.summary, needsQuotes(event.summary)) || !csvComma(out, capacity, used)) return false;
    if (!csvAppend(out, capacity, used, event.ssid, needsQuotes(event.ssid)) || !csvComma(out, capacity, used)) return false;
    if (!csvAppend(out, capacity, used, event.bssid, needsQuotes(event.bssid)) || !csvComma(out, capacity, used)) return false;
    if (!csvAppend(out, capacity, used, event.ble_address, needsQuotes(event.ble_address)) || !csvComma(out, capacity, used)) return false;
    if (event.flags & kFlagRssi) std::snprintf(number, sizeof(number), "%d", event.rssi);
    else number[0] = 0;
    if (!csvAppend(out, capacity, used, number, false) || !csvComma(out, capacity, used)) return false;
    if (event.flags & kFlagChannel) std::snprintf(number, sizeof(number), "%d", event.channel);
    else number[0] = 0;
    if (!csvAppend(out, capacity, used, number, false)) return false;
    if (used + 2 >= capacity) return false;
    out[used++] = '\n';
    out[used] = 0;
    length = used;
    return true;
}
bool formatIntegritySidecar(const char* filename, const uint8_t hash[32], uint64_t size, char* out, size_t capacity) {
    if (!out || !filename || !hash || !capacity) return false;
    char hex[65];
    sha256Hex(hash, hex);
    const int wrote = std::snprintf(out, capacity,
                                    "integrity checksum\nsha256 %s\nfile %s\nbytes %llu\n",
                                    hex, filename, static_cast<unsigned long long>(size));
    return wrote > 0 && static_cast<size_t>(wrote) < capacity && std::strstr(out, "integrity checksum") &&
           !std::strstr(out, "tamper-proof") && !std::strstr(out, "chain-of-custody");
}
namespace {
bool appendText(char* out, size_t capacity, size_t& used, const char* text) {
    if (!text) return false;
    const size_t length = std::strlen(text);
    if (used + length >= capacity) return false;
    std::memcpy(out + used, text, length);
    used += length;
    out[used] = 0;
    return true;
}
}
size_t formatDiagnostics(const DiagnosticSnapshot& snapshot, char* out, size_t capacity) {
    if (!out || capacity < 32) return 0;
    char line[160];
    size_t used = 0;
    out[0] = 0;
    char block[24][96];
    std::snprintf(block[0], sizeof(block[0]), "firmware: %s\n", snapshot.firmware ? snapshot.firmware : "");
    std::snprintf(block[1], sizeof(block[1]), "board: %s\n", snapshot.board ? snapshot.board : "");
    std::snprintf(block[2], sizeof(block[2]), "session_id: %lu\n", static_cast<unsigned long>(snapshot.session_id));
    std::snprintf(block[3], sizeof(block[3]), "reset_reason: %s\n", snapshot.reset_reason ? snapshot.reset_reason : "unknown");
    std::snprintf(block[4], sizeof(block[4]), "flash_size: %lu\n", static_cast<unsigned long>(snapshot.flash_size));
    std::snprintf(block[5], sizeof(block[5]), "psram_size: %lu\n", static_cast<unsigned long>(snapshot.psram_size));
    std::snprintf(block[6], sizeof(block[6]), "free_heap: %lu\n", static_cast<unsigned long>(snapshot.free_heap));
    std::snprintf(block[7], sizeof(block[7]), "free_psram: %lu\n", static_cast<unsigned long>(snapshot.free_psram));
    std::snprintf(block[8], sizeof(block[8]), "min_free_heap: %lu\n", static_cast<unsigned long>(snapshot.min_heap));
    std::snprintf(block[9], sizeof(block[9]), "min_free_psram: %lu\n", static_cast<unsigned long>(snapshot.min_psram));
    std::snprintf(block[10], sizeof(block[10]), "sd_state: %s\n", snapshot.sd_state ? snapshot.sd_state : "");
    std::snprintf(block[11], sizeof(block[11]), "sd_card: %s\n", snapshot.sd_card ? snapshot.sd_card : "");
    std::snprintf(block[12], sizeof(block[12]), "sd_total_bytes: %llu\n", static_cast<unsigned long long>(snapshot.sd_total));
    std::snprintf(block[13], sizeof(block[13]), "sd_free_bytes: %llu\n", static_cast<unsigned long long>(snapshot.sd_free));
    std::snprintf(block[14], sizeof(block[14]), "mount_attempts: %lu\n", static_cast<unsigned long>(snapshot.mount_attempts));
    std::snprintf(block[15], sizeof(block[15]), "mount_failures: %lu\n", static_cast<unsigned long>(snapshot.mount_failures));
    std::snprintf(block[16], sizeof(block[16]), "write_failures: %lu\n", static_cast<unsigned long>(snapshot.write_failures));
    std::snprintf(block[17], sizeof(block[17]), "flush_failures: %lu\n", static_cast<unsigned long>(snapshot.flush_failures));
    std::snprintf(block[18], sizeof(block[18]), "events_written: %lu\n", static_cast<unsigned long>(snapshot.events_written));
    std::snprintf(block[19], sizeof(block[19]), "events_dropped: %lu\n", static_cast<unsigned long>(snapshot.events_dropped));
    std::snprintf(block[20], sizeof(block[20]), "event_queue_capacity: %lu\n", static_cast<unsigned long>(snapshot.queue_capacity));
    std::snprintf(block[21], sizeof(block[21]), "event_queue_high_water: %lu\n", static_cast<unsigned long>(snapshot.queue_high_water));
    std::snprintf(block[22], sizeof(block[22]), "wifi_queue_drops: %lu\n", static_cast<unsigned long>(snapshot.wifi_queue_drops));
    std::snprintf(block[23], sizeof(block[23]), "ble_queue_drops: %lu\n", static_cast<unsigned long>(snapshot.ble_queue_drops));
    for (int i = 0; i < 24; ++i) if (!appendText(out, capacity, used, block[i])) return 0;
    std::snprintf(line, sizeof(line), "radio_transition_failures: %lu\nlogging: %s\nsummaries: %s\nretention: %s\nintegrity_checksum: %s\n",
                  static_cast<unsigned long>(snapshot.radio_failures),
                  snapshot.logging_enabled ? "on" : "off",
                  snapshot.summaries_enabled ? "on" : "off",
                  snapshot.retention_enabled ? "on" : "off",
                  snapshot.checksum_enabled ? "on" : "off");
    if (!appendText(out, capacity, used, line)) return 0;
    if (std::strstr(out, "password") || std::strstr(out, "credential")) return 0;
    return used;
}
