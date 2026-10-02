#pragma once
#include "log_policy.h"

// Owns the SD_MMC lifecycle for the ES3C28P. Event producers never call this.
class StorageManager {
public:
    bool begin();
    void end();
    bool ready() const;
    StorageState state() const;
    uint64_t totalBytes() const;
    uint64_t usedBytes() const;
    uint64_t freeBytes() const;
    const char* cardTypeName() const;
    uint32_t mountAttempts() const;
    uint32_t mountFailures() const;
    int dataBits() const { return kSdDataBits; }
    bool openSession(uint32_t session_id);
    bool writeSession(const char* data, size_t length);
    bool flushSession();
    void closeSession();
    bool sessionOpen() const;
    uint32_t highestSessionId() const;
    bool readTail(uint32_t session_id, char* buffer, size_t capacity, size_t& length, bool& from_start);
    bool copySession(uint32_t session_id, const char* destination, void (*pump)());
    bool writeCsvExport(uint32_t session_id, const char* destination, uint32_t& skipped, void (*pump)());
    bool hashSession(uint32_t session_id, uint8_t hash[32], uint64_t& size);
    bool writeWholeFile(const char* path, const char* data, size_t length);
    bool removeOldestSession(uint32_t current_session, uint32_t max_files, bool low_space_already_warned, uint32_t& removed_id);
    void markFaulted();
};

StorageManager& goblinStorage();
