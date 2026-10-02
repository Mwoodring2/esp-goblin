#include "storage_manager.h"
#include "es3c28p_board.h"
#include <SD_MMC.h>
#include <Arduino.h>
#include <cstring>

namespace {
StorageState storage_state = StorageState::Uninitialized;
File session_file;
uint32_t mount_attempts = 0;
uint32_t mount_failures = 0;
bool driver_started = false;
char card_name[16] = "none";
char session_path[64] = {};

const char* baseName(const char* path) {
    const char* name = path ? path : "";
    for (const char* cursor = name; *cursor; ++cursor) if (*cursor == '/') name = cursor + 1;
    return name;
}
bool sessionPath(uint32_t session_id, char* out, size_t capacity) {
    char file[32];
    return buildSessionFileName(session_id, file, sizeof(file)) &&
           buildGoblinFilePath(out, capacity, "sessions", file);
}
bool sidecarPath(uint32_t session_id, char* out, size_t capacity) {
    char file[32];
    return buildSidecarFileName(session_id, file, sizeof(file)) &&
           buildGoblinFilePath(out, capacity, "sessions", file);
}
const char* cardTypeText(uint8_t type) {
    if (type == CARD_MMC) return "MMC";
    if (type == CARD_SD) return "SD";
    if (type == CARD_SDHC) return "SDHC";
    return "none";
}
bool ensureTree() {
    const char* dirs[] = {"/goblin", "/goblin/config", "/goblin/logs", "/goblin/sessions", "/goblin/export", "/goblin/diagnostics"};
    for (size_t i = 0; i < 6; ++i) {
        if (SD_MMC.exists(dirs[i])) continue;
        if (!SD_MMC.mkdir(dirs[i])) return false;
    }
    return true;
}
void closeQuietly() {
    if (session_file) session_file.close();
    session_path[0] = 0;
}
}

bool StorageManager::begin() {
    ++mount_attempts;
    closeQuietly();
    if (driver_started) {
        SD_MMC.end();
        driver_started = false;
    }
    storage_state = StorageState::Mounting;
    // 1-bit until the ES3C28P SD bus is proven. Width-4 stays available on the
    // board profile (D1/D2/D3) but is intentionally not selected here.
    if (!SD_MMC.setPins(GoblinBoard::SD_CLK, GoblinBoard::SD_CMD, GoblinBoard::SD_D0)) {
        storage_state = StorageState::MountFailed;
        ++mount_failures;
        Serial.println("[sd] pin assignment failed");
        return false;
    }
    if (!SD_MMC.begin("/sdcard", true, false, kSdBusFrequencyKhz)) {
        storage_state = StorageState::NoCard;
        ++mount_failures;
        std::strncpy(card_name, "none", sizeof(card_name) - 1);
        Serial.println("[sd] mount failed; Guard, Air, and BLE keep running");
        return false;
    }
    driver_started = true;
    const uint8_t type = SD_MMC.cardType();
    if (type == CARD_NONE) {
        storage_state = StorageState::NoCard;
        ++mount_failures;
        std::strncpy(card_name, "none", sizeof(card_name) - 1);
        Serial.println("[sd] no card detected");
        return false;
    }
    std::strncpy(card_name, cardTypeText(type), sizeof(card_name) - 1);
    card_name[sizeof(card_name) - 1] = 0;
    if (!ensureTree()) {
        storage_state = StorageState::ReadOnly;
        ++mount_failures;
        Serial.println("[sd] goblin directories are not writable");
        return false;
    }
    storage_state = StorageState::Ready;
    Serial.printf("[sd] mounted %d-bit %s total=%llu free=%llu\n", kSdDataBits, card_name,
                  static_cast<unsigned long long>(totalBytes()), static_cast<unsigned long long>(freeBytes()));
    return true;
}
void StorageManager::end() {
    closeQuietly();
    if (driver_started) SD_MMC.end();
    driver_started = false;
    storage_state = StorageState::Uninitialized;
}
bool StorageManager::ready() const { return storage_state == StorageState::Ready; }
StorageState StorageManager::state() const { return storage_state; }
uint64_t StorageManager::totalBytes() const { return driver_started ? SD_MMC.totalBytes() : 0; }
uint64_t StorageManager::usedBytes() const { return driver_started ? SD_MMC.usedBytes() : 0; }
uint64_t StorageManager::freeBytes() const {
    const uint64_t total = totalBytes();
    const uint64_t used = usedBytes();
    return used < total ? total - used : 0;
}
const char* StorageManager::cardTypeName() const { return card_name; }
uint32_t StorageManager::mountAttempts() const { return mount_attempts; }
uint32_t StorageManager::mountFailures() const { return mount_failures; }
bool StorageManager::openSession(uint32_t session_id) {
    if (storage_state != StorageState::Ready) return false;
    closeQuietly();
    if (!sessionPath(session_id, session_path, sizeof(session_path))) return false;
    session_file = SD_MMC.open(session_path, FILE_APPEND);
    if (!session_file) {
        storage_state = StorageState::Faulted;
        session_path[0] = 0;
        Serial.printf("[sd] cannot open session %lu\n", static_cast<unsigned long>(session_id));
        return false;
    }
    return true;
}
bool StorageManager::writeSession(const char* data, size_t length) {
    if (storage_state != StorageState::Ready || !session_file || !data) return false;
    const size_t wrote = session_file.write(reinterpret_cast<const uint8_t*>(data), length);
    if (wrote != length) {
        markFaulted();
        Serial.println("[sd] write failed; logging paused until RETRY SD");
        return false;
    }
    return true;
}
bool StorageManager::flushSession() {
    if (!session_file) return false;
    session_file.flush();
    return storage_state == StorageState::Ready;
}
void StorageManager::closeSession() { closeQuietly(); }
bool StorageManager::sessionOpen() const { return static_cast<bool>(session_file); }
uint32_t StorageManager::highestSessionId() const {
    if (!driver_started) return 0;
    File dir = SD_MMC.open("/goblin/sessions");
    if (!dir) return 0;
    uint32_t highest = 0;
    for (unsigned guard = 0; guard < 400; ++guard) {
        File entry = dir.openNextFile();
        if (!entry) break;
        uint32_t id = 0;
        if (parseSessionFileName(baseName(entry.name()), id) && id > highest) highest = id;
        entry.close();
    }
    dir.close();
    return highest;
}
bool StorageManager::readTail(uint32_t session_id, char* buffer, size_t capacity, size_t& length, bool& from_start) {
    length = 0;
    from_start = true;
    if (!buffer || capacity < 2 || !driver_started) return false;
    char path[64];
    if (!sessionPath(session_id, path, sizeof(path))) return false;
    File file = SD_MMC.open(path, FILE_READ);
    if (!file) return false;
    const size_t size = file.size();
    const size_t take = size < capacity - 1 ? size : capacity - 1;
    from_start = take == size;
    if (size > take && !file.seek(size - take)) { file.close(); return false; }
    const int got = file.read(reinterpret_cast<uint8_t*>(buffer), take);
    file.close();
    if (got < 0) return false;
    length = static_cast<size_t>(got);
    buffer[length] = 0;
    return true;
}
bool StorageManager::copySession(uint32_t session_id, const char* destination, void (*pump)()) {
    if (!driver_started || !pathInsideGoblin(destination)) return false;
    char path[64];
    if (!sessionPath(session_id, path, sizeof(path))) return false;
    File input = SD_MMC.open(path, FILE_READ);
    File output = SD_MMC.open(destination, FILE_WRITE);
    if (!input || !output) { if (input) input.close(); if (output) output.close(); return false; }
    uint8_t chunk[256];
    size_t since_pump = 0;
    bool ok = true;
    while (ok) {
        const int got = input.read(chunk, sizeof(chunk));
        if (got < 0) ok = false;
        else if (got == 0) break;
        else if (output.write(chunk, static_cast<size_t>(got)) != static_cast<size_t>(got)) ok = false;
        else {
            since_pump += static_cast<size_t>(got);
            if (pump && since_pump >= 4096) { pump(); since_pump = 0; }
        }
    }
    output.flush();
    input.close();
    output.close();
    return ok;
}
bool StorageManager::writeCsvExport(uint32_t session_id, const char* destination, uint32_t& skipped, void (*pump)()) {
    skipped = 0;
    if (!driver_started || !pathInsideGoblin(destination)) return false;
    char path[64];
    if (!sessionPath(session_id, path, sizeof(path))) return false;
    File input = SD_MMC.open(path, FILE_READ);
    File output = SD_MMC.open(destination, FILE_WRITE);
    if (!input || !output) { if (input) input.close(); if (output) output.close(); return false; }
    const char* header = csvHeader();
    bool ok = output.write(reinterpret_cast<const uint8_t*>(header), std::strlen(header)) == std::strlen(header);
    static char line[kJsonLineCapacity];
    size_t used = 0;
    bool discard = false;
    size_t since_pump = 0;
    while (ok) {
        uint8_t value = 0;
        const int got = input.read(&value, 1);
        if (got < 0) { ok = false; break; }
        if (got == 0) break;
        ++since_pump;
        if (value == '\n') {
            line[used] = 0;
            if (!discard && used) {
                EventRecord event;
                char row[512];
                size_t row_len = 0;
                if (!eventFromJsonLine(line, event) || !csvRowFromEvent(event, row, sizeof(row), row_len)) ++skipped;
                else if (output.write(reinterpret_cast<const uint8_t*>(row), row_len) != row_len) ok = false;
            } else if (discard) ++skipped;
            used = 0;
            discard = false;
        } else if (!discard) {
            if (used + 1 >= sizeof(line)) discard = true;
            else line[used++] = static_cast<char>(value);
        }
        if (pump && since_pump >= 4096) { pump(); since_pump = 0; }
    }
    output.flush();
    input.close();
    output.close();
    return ok;
}
bool StorageManager::hashSession(uint32_t session_id, uint8_t hash[32], uint64_t& size) {
    size = 0;
    if (!hash || !driver_started) return false;
    char path[64];
    if (!sessionPath(session_id, path, sizeof(path))) return false;
    File input = SD_MMC.open(path, FILE_READ);
    if (!input) return false;
    Sha256 sha;
    uint8_t chunk[256];
    while (true) {
        const int got = input.read(chunk, sizeof(chunk));
        if (got < 0) { input.close(); return false; }
        if (got == 0) break;
        sha.update(chunk, static_cast<size_t>(got));
        size += static_cast<uint64_t>(got);
    }
    input.close();
    sha.finish(hash);
    return true;
}
bool StorageManager::writeWholeFile(const char* path, const char* data, size_t length) {
    if (!driver_started || !pathInsideGoblin(path) || !data) return false;
    File output = SD_MMC.open(path, FILE_WRITE);
    if (!output) return false;
    const bool ok = output.write(reinterpret_cast<const uint8_t*>(data), length) == length;
    output.flush();
    output.close();
    return ok;
}
bool StorageManager::removeOldestSession(uint32_t current_session, uint32_t max_files,
                                         bool low_space_already_warned, uint32_t& removed_id) {
    removed_id = 0;
    if (storage_state != StorageState::Ready) return false;
    File dir = SD_MMC.open("/goblin/sessions");
    if (!dir) return false;
    uint32_t count = 0;
    uint32_t oldest = 0;
    bool found = false;
    for (unsigned guard = 0; guard < 400; ++guard) {
        File entry = dir.openNextFile();
        if (!entry) break;
        uint32_t id = 0;
        if (parseSessionFileName(baseName(entry.name()), id)) {
            ++count;
            if (id != current_session && (!found || id < oldest)) { oldest = id; found = true; }
        }
        entry.close();
    }
    dir.close();
    if (!retentionNeeded(count, max_files, freeBytes(), kSdFreeReserveBytes, low_space_already_warned) || !found) return false;
    char path[64];
    if (!sessionPath(oldest, path, sizeof(path)) || !SD_MMC.remove(path)) return false;
    char sidecar[64];
    if (sidecarPath(oldest, sidecar, sizeof(sidecar))) SD_MMC.remove(sidecar);
    removed_id = oldest;
    Serial.printf("[sd] retention removed session %lu\n", static_cast<unsigned long>(oldest));
    return true;
}
void StorageManager::markFaulted() {
    storage_state = StorageState::Faulted;
    closeQuietly();
}
StorageManager& goblinStorage() {
    static StorageManager storage;
    return storage;
}
