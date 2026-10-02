#include "event_logger.h"
#include "log_policy.h"
#include <cstdio>
#include <cstring>

static int failures = 0;
static int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
class MemorySessionStore : public SessionIdStore {
public:
    bool fail_read = false;
    bool fail_write = false;
    bool present = false;
    uint32_t next = 1;
    bool readNext(uint32_t& value) override {
        if (fail_read) return false;
        value = present ? next : 1;
        return true;
    }
    bool writeNext(uint32_t value) override {
        if (fail_write) return false;
        present = true;
        next = value;
        return true;
    }
};
void fillText(char* dest, const char* value) {
    uint16_t flags = 0;
    eventCopyText(dest, 80, value, flags);
}
int main() {
    EventRecord event;
    event.session_id = 42;
    event.event_id = 17;
    event.monotonic_ms = 482912;
    event.category = EventCategory::Guard;
    event.type = EventType::SsidCollision;
    event.severity = EventSeverity::Warning;
    event.rssi = -52;
    event.channel = 6;
    event.flags = static_cast<uint16_t>(kFlagRssi | kFlagChannel);
    fillText(event.summary, "SSID collision");
    fillText(event.ssid, "Home\"WiFi");
    fillText(event.bssid, "11:22:33:44:55:66");
    fillText(event.trusted_bssid, "AA:BB:CC:DD:EE:FF");
    char line[kJsonLineCapacity];
    size_t length = 0;
    check(eventToJsonLine(event, 0, line, sizeof(line), length) && line[length - 1] == '\n', "jsonl line ends with newline");
    check(std::strstr(line, "\"schema\":1") && std::strstr(line, "\"session\":42") && std::strstr(line, "\"event\":17"), "schema session and event ids");
    check(std::strstr(line, "\"time_valid\":false") && !std::strstr(line, "unix_ms"), "invalid time omits a fabricated unix timestamp");
    check(std::strstr(line, "Home\\\"WiFi"), "ssid quotes are escaped");
    check(!eventToJsonLine(event, 0, line, 16, length), "short buffer does not emit a partial record");
    EventRecord parsed;
    check(eventToJsonLine(event, 0, line, sizeof(line), length) && eventFromJsonLine(line, parsed), "jsonl round trip parses");
    check(parsed.session_id == 42 && parsed.event_id == 17 && parsed.type == EventType::SsidCollision, "parsed identity");
    check(!std::strcmp(parsed.ssid, "Home\"WiFi") && parsed.rssi == -52 && parsed.channel == 6, "parsed ssid rssi and channel");
    check(!eventFromJsonLine("{\"schema\":1,\"session\":1", parsed), "truncated json line is rejected");

    event.flags = static_cast<uint16_t>(event.flags | kFlagTimeValid);
    event.unix_ms = 1609459200000ull;
    event.type = EventType::TimeSync;
    event.category = EventCategory::System;
    fillText(event.source, "system");
    check(eventToJsonLine(event, 0, line, sizeof(line), length) && std::strstr(line, "\"type\":\"TIME_SYNC\"") &&
          std::strstr(line, "\"time_valid\":true") && std::strstr(line, "\"unix_ms\":1609459200000") &&
          std::strstr(line, "\"source\":\"system\""), "valid TIME_SYNC carries unix time and source");

    uint16_t flags = 0;
    char bounded[33];
    eventCopyText(bounded, sizeof(bounded), "0123456789012345678901234567890123456789", flags);
    check((flags & kFlagTruncated) && std::strlen(bounded) == 32, "overlong text is bounded and marked truncated");
    EventRecord truncated = event;
    truncated.flags = flags;
    fillText(truncated.ssid, bounded);
    check(eventToJsonLine(truncated, 0, line, sizeof(line), length) && std::strstr(line, "\"truncated\":true"), "truncation is visible in json");

    SessionFacts facts = {};
    facts.firmware = "0.1.0-alpha.0";
    facts.board_id = "hosyond_es3c28p";
    facts.reset_reason = "power_on";
    facts.sd_card_type = "SDHC";
    facts.flash_size = 16;
    facts.psram_size = 8;
    facts.trusted_ap_count = 2;
    facts.wifi_monitor_enabled = false;
    facts.ble_privacy_enabled = true;
    EventRecord session;
    session.type = EventType::SessionStart;
    session.session_id = 3;
    session.event_id = 1;
    fillText(session.summary, "session start");
    check(eventToJsonLine(session, &facts, line, sizeof(line), length) && std::strstr(line, "\"firmware_version\":\"0.1.0-alpha.0\"") &&
          std::strstr(line, "\"reset_reason\":\"power_on\"") && std::strstr(line, "\"ble_privacy_enabled\":true") &&
          !std::strstr(line, "password"), "session header has environment facts and no secrets");

    EventLogger logger;
    for (uint32_t index = 0; index < kEventQueueCapacity; ++index) {
        EventRecord item;
        item.type = EventType::NewAccessPoint;
        item.monotonic_ms = index;
        std::snprintf(item.bssid, sizeof(item.bssid), "02:00:00:00:%02lu:%02lu",
                      static_cast<unsigned long>(index / 50), static_cast<unsigned long>(index % 50));
        check(logger.submit(item) == LogResult::Queued && item.event_id == index + 1, "event ids increase inside the session");
    }
    check(logger.highWater() == kEventQueueCapacity && logger.queued() == kEventQueueCapacity, "queue high water reaches capacity");
    EventRecord extra;
    extra.type = EventType::Boot;
    extra.monotonic_ms = 1000;
    check(logger.submit(extra) == LogResult::Dropped && logger.dropped() == 1 && logger.queued() == kEventQueueCapacity, "full queue drops and counts");
    EventRecord first;
    check(logger.peek(first) && first.event_id == 1, "oldest queued event remains");
    while (logger.queued()) logger.pop();
    check(logger.highWater() == kEventQueueCapacity && logger.queued() == 0, "high water remains after drain");

    EventLogger deduped;
    EventRecord collision;
    collision.type = EventType::SsidCollision;
    collision.monotonic_ms = 10;
    fillText(collision.bssid, "11:22:33:44:55:66");
    fillText(collision.trusted_bssid, "AA:BB:CC:DD:EE:FF");
    check(deduped.submit(collision) == LogResult::Queued, "first collision is kept");
    collision.monotonic_ms = 20;
    check(deduped.submit(collision) == LogResult::Suppressed && deduped.nextId() == 2, "duplicate collision does not consume an id");
    EventRecord low;
    low.type = EventType::SdLowSpace;
    low.monotonic_ms = 0;
    check(deduped.submit(low) == LogResult::Queued, "first low-space warning is kept");
    low.monotonic_ms = 1000;
    check(deduped.submit(low) == LogResult::Suppressed, "low-space warning is inside cooldown");
    low.monotonic_ms = kDedupCooldownMs;
    check(deduped.submit(low) == LogResult::Queued && low.event_id == 3, "cooldown expiry admits the warning again");

    EventHistory history;
    for (uint32_t index = 1; index <= kHistoryCapacity + 5; ++index) {
        EventRecord item;
        item.event_id = index;
        history.push(item);
    }
    check(history.count() == kHistoryCapacity && history.newest(0) && history.newest(0)->event_id == kHistoryCapacity + 5, "history keeps the newest record");
    check(history.newest(kHistoryCapacity - 1) && history.newest(kHistoryCapacity - 1)->event_id == 6, "history drops the oldest records");
    check(!history.newest(kHistoryCapacity), "history offset past the end is empty");

    uint32_t ids[] = {5, 1, 3, 5};
    uint32_t oldest = 0;
    check(selectOldestSession(ids, 4, 5, oldest) && oldest == 1, "oldest completed session is selected by id");
    check(!selectOldestSession(ids, 4, 1, oldest) || oldest != 1, "a candidate other than current can be selected");
    uint32_t only_current[] = {4, 4};
    check(!selectOldestSession(only_current, 2, 4, oldest), "current session is never selected for deletion");
    check(!retentionNeeded(10, kMaxSessionFiles, 0, kSdFreeReserveBytes, false), "first low-space observation does not delete");
    check(retentionNeeded(10, kMaxSessionFiles, 0, kSdFreeReserveBytes, true), "later low-space observation may delete");
    check(retentionNeeded(kMaxSessionFiles + 1, kMaxSessionFiles, kSdFreeReserveBytes, kSdFreeReserveBytes, false), "too many session files require retention");
    const uint64_t large = 32ull << 30;
    check(storageIsLow(kSdFreeReserveBytes - 1, large, kSdFreeReserveBytes, kSdLowSpacePercent), "large card uses the 64 MB reserve");
    check(!storageIsLow(kSdFreeReserveBytes, large, kSdFreeReserveBytes, kSdLowSpacePercent), "reserve exactly met is not low");
    check(!storageIsLow(4ull << 20, 32ull << 20, kSdFreeReserveBytes, kSdLowSpacePercent), "small card uses the lower percentage limit");

    char name[40], path[80];
    check(buildSessionFileName(42, name, sizeof(name)) && !std::strcmp(name, "session_000042.jsonl"), "session filename");
    check(buildGoblinFilePath(path, sizeof(path), "sessions", name) && !std::strcmp(path, "/goblin/sessions/session_000042.jsonl"), "session path stays under goblin");
    check(pathInsideGoblin(path) && !pathInsideGoblin("/goblin/../secret") && !pathInsideGoblin("/sdcard/goblin/sessions/../../etc/passwd"), "path traversal is rejected");
    check(!buildGoblinFilePath(path, sizeof(path), "sessions", "../session_000001.jsonl"), "file names cannot carry traversal");
    check(!buildGoblinFilePath(path, sizeof(path), "sessions", "Home/WiFi"), "ssid text is not a filename");
    uint32_t parsed_id = 0;
    check(parseSessionFileName(name, parsed_id) && parsed_id == 42, "session id parses from its filename");
    check(!parseSessionFileName("../session_000001.jsonl", parsed_id) && !parseSessionFileName("session_000042.jsonl.bak", parsed_id), "non-session names are rejected");

    MemorySessionStore store;
    store.next = 4;
    store.present = true;
    SessionAssignment assigned = assignSessionId(store, 9);
    check(assigned.id == 10 && assigned.persisted && store.next == 11, "session id advances past both the counter and existing files");
    store.fail_read = true;
    assigned = assignSessionId(store, 2);
    check(assigned.id == 3, "storage failure falls forward from existing session files");
    store.fail_read = false;
    store.fail_write = true;
    store.present = false;
    assigned = assignSessionId(store, 0);
    check(assigned.id == 1 && !assigned.persisted, "counter write failure is reported and still yields an id");

    char clock[20];
    formatEvidenceTime(false, 0, 512000, 0, clock, sizeof(clock));
    check(!std::strcmp(clock, "+08m32s"), "monotonic time renders as an offset");
    formatEvidenceTime(false, 123, 0, 0, clock, sizeof(clock));
    check(!std::strcmp(clock, "+00m00s"), "invalid wall time does not become a clock");
    formatEvidenceTime(true, 1609459200000ull, 5, 0, clock, sizeof(clock));
    check(!std::strcmp(clock, "00:00 UTC"), "plausible unix time renders as UTC");

    EventRecord row = event;
    row.summary[0] = 0;
    fillText(row.summary, "say \"hi\", now");
    char csv[256];
    size_t csv_length = 0;
    check(csvRowFromEvent(row, csv, sizeof(csv), csv_length) && std::strstr(csv, "\"say \"\"hi\"\", now\""), "csv quotes and commas are escaped");
    check(std::strstr(csvHeader(), "session_id,event_id,timestamp"), "csv header has the common columns");
    check(inspectLogTail("", 0) == LogTail::Empty && inspectLogTail("{\"a\":1}", 7) == LogTail::Truncated &&
          inspectLogTail("{\"a\":1}\n", 8) == LogTail::Complete, "only a trailing incomplete line is truncated");
    check(logFlushDue(1000, 0, 0, false, true) && logFlushDue(1000, 0, 1, true, false) &&
          logFlushDue(5000, 0, 1, false, false) && !logFlushDue(1000, 0, 1, false, false), "flush policy");
    check(storageStateAfterMount(false, false, false) == StorageState::NoCard, "missing card is not a fatal mount");
    check(storageStateAfterMount(true, false, false) == StorageState::MountFailed, "failed mount is explicit");
    check(storageStateAfterWrite(StorageState::Ready, false) == StorageState::Faulted, "write failure faults ready storage");
    check(storageStateAfterWrite(StorageState::NoCard, false) == StorageState::NoCard, "write failure does not invent a card");
    check(!std::strcmp(resetReasonName(1), "power_on") && !resetReasonUnexpected(1), "power-on reset is expected");
    check(!std::strcmp(resetReasonName(4), "panic") && resetReasonUnexpected(4) && !std::strcmp(resetReasonName(99), "unknown"), "panic and unknown reset names");
    check(!std::strcmp(storageUiLabel(StorageState::Ready, false), "SD OK") && !std::strcmp(storageUiLabel(StorageState::Ready, true), "SD LOW") &&
          !std::strcmp(storageUiLabel(StorageState::Faulted, false), "SD ERR") && !std::strcmp(storageUiLabel(StorageState::NoCard, false), "SD --"), "storage labels");

    Sha256 sha;
    uint8_t digest[32];
    sha.finish(digest);
    char hex[65];
    sha256Hex(digest, hex);
    check(!std::strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "sha256 empty vector");
    Sha256 abc;
    abc.update("abc", 3);
    abc.finish(digest);
    sha256Hex(digest, hex);
    check(!std::strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "sha256 abc vector");
    char sidecar[180];
    check(formatIntegritySidecar("session_000042.jsonl", digest, 12, sidecar, sizeof(sidecar)) &&
          std::strstr(sidecar, "integrity checksum") && std::strstr(sidecar, "session_000042.jsonl") &&
          !std::strstr(sidecar, "tamper-proof"), "sidecar is an integrity checksum");

    DiagnosticSnapshot snapshot = {};
    snapshot.firmware = "0.1.0-alpha.0";
    snapshot.board = "hosyond_es3c28p";
    snapshot.reset_reason = "power_on";
    snapshot.sd_state = "ready";
    snapshot.sd_card = "SDHC";
    snapshot.session_id = 42;
    snapshot.queue_capacity = static_cast<uint32_t>(kEventQueueCapacity);
    snapshot.logging_enabled = true;
    snapshot.summaries_enabled = true;
    snapshot.retention_enabled = true;
    snapshot.checksum_enabled = true;
    char diagnostics[2048];
    const size_t diagnostics_length = formatDiagnostics(snapshot, diagnostics, sizeof(diagnostics));
    check(diagnostics_length && std::strstr(diagnostics, "firmware:") && std::strstr(diagnostics, "session_id: 42") &&
          std::strstr(diagnostics, "wifi_queue_drops:") && std::strstr(diagnostics, "ble_queue_drops:") &&
          !std::strstr(diagnostics, "password"), "diagnostics include field data and no secrets");

    std::printf("event log checks %d failures %d\n", checks, failures);
    return failures ? 1 : 0;
}
