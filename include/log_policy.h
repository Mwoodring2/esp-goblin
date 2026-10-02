#pragma once
#include "event_record.h"

enum class StorageState : uint8_t {
    Uninitialized, Mounting, Ready, NoCard, MountFailed, ReadOnly, Faulted
};
enum class LogTail : uint8_t { Empty, Complete, Truncated };

struct SessionAssignment {
    uint32_t id = 0;
    bool persisted = false;
};
class SessionIdStore {
public:
    virtual ~SessionIdStore() {}
    virtual bool readNext(uint32_t& next) = 0;
    virtual bool writeNext(uint32_t next) = 0;
};

struct DiagnosticSnapshot {
    const char* firmware;
    const char* board;
    const char* reset_reason;
    const char* sd_state;
    const char* sd_card;
    uint32_t session_id;
    uint32_t flash_size;
    uint32_t psram_size;
    uint32_t free_heap;
    uint32_t free_psram;
    uint32_t min_heap;
    uint32_t min_psram;
    uint64_t sd_total;
    uint64_t sd_free;
    uint32_t mount_attempts;
    uint32_t mount_failures;
    uint32_t write_failures;
    uint32_t flush_failures;
    uint32_t events_written;
    uint32_t events_dropped;
    uint32_t queue_capacity;
    uint32_t queue_high_water;
    uint32_t wifi_queue_drops;
    uint32_t ble_queue_drops;
    uint32_t radio_failures;
    bool logging_enabled;
    bool summaries_enabled;
    bool retention_enabled;
    bool checksum_enabled;
};

const char* storageStateName(StorageState state);
const char* storageUiLabel(StorageState state, bool low_space);
StorageState storageStateAfterMount(bool card_seen, bool mounted, bool writable);
StorageState storageStateAfterWrite(StorageState state, bool write_ok);
const char* resetReasonName(int code);
bool resetReasonUnexpected(int code);
bool wallClockPlausible(uint64_t unix_ms);
uint32_t nextSessionId(uint32_t stored_next, bool stored_ok, uint32_t highest_file_id);
SessionAssignment assignSessionId(SessionIdStore& store, uint32_t highest_file_id);
uint64_t lowSpaceLimit(uint64_t total_bytes, uint64_t reserve_bytes, uint8_t percent);
bool storageIsLow(uint64_t free_bytes, uint64_t total_bytes, uint64_t reserve_bytes, uint8_t percent);
bool retentionNeeded(uint32_t session_files, uint32_t max_files, uint64_t free_bytes,
                     uint64_t reserve_bytes, bool low_space_already_warned);
bool selectOldestSession(const uint32_t* ids, size_t count, uint32_t current, uint32_t& oldest);
bool logFlushDue(uint32_t now_ms, uint32_t last_flush_ms, uint32_t pending, bool important, bool force);
LogTail inspectLogTail(const char* data, size_t length);
bool pathInsideGoblin(const char* path);
bool buildSessionFileName(uint32_t id, char* out, size_t capacity);
bool buildSidecarFileName(uint32_t id, char* out, size_t capacity);
bool buildExportFileName(uint32_t id, const char* extension, char* out, size_t capacity);
bool buildGoblinFilePath(char* out, size_t capacity, const char* folder, const char* file);
bool parseSessionFileName(const char* name, uint32_t& id);
void formatEvidenceTime(bool time_valid, uint64_t unix_ms, uint32_t monotonic_ms, uint32_t origin_ms,
                        char* out, size_t capacity);
void formatByteCount(uint64_t bytes, char* out, size_t capacity);
bool csvRowFromEvent(const EventRecord& event, char* out, size_t capacity, size_t& length);
const char* csvHeader();
bool formatIntegritySidecar(const char* filename, const uint8_t hash[32], uint64_t size, char* out, size_t capacity);
size_t formatDiagnostics(const DiagnosticSnapshot& snapshot, char* out, size_t capacity);

struct Sha256 {
    Sha256();
    void reset();
    void update(const void* data, size_t length);
    void finish(uint8_t out[32]);
    uint64_t bits_ = 0;
    uint32_t state_[8] = {};
    uint8_t block_[64] = {};
    size_t block_len_ = 0;
};
void sha256Hex(const uint8_t hash[32], char out[65]);
