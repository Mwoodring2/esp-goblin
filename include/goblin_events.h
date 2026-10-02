#pragma once
#include "event_record.h"
#include "guard_analyzer.h"
#include <stddef.h>
#include <stdint.h>

void goblinEventsBegin();
void goblinEventsTick();
void goblinEventsShutdown();
void goblinLogGuardAlerts();
void goblinLogDisconnect(const DisconnectBurst& burst, uint8_t subtype);
void goblinLogBleAlert(const BleDeviceRecord& record, bool persistent);
void goblinLogDismiss(const GuardAlert& alert);
void goblinLogTrust(bool added, const uint8_t* bssid, const char* ssid);
void goblinLogForget(const uint8_t* bssid, const char* ssid);
void goblinLogConfig(const char* summary);

const char* goblinStorageLabel();
uint32_t goblinSessionId();
uint32_t goblinEventsWritten();
uint32_t goblinEventsDropped();
uint32_t goblinLoggerHighWater();
uint32_t goblinLoggerQueued();
uint64_t goblinSdTotalBytes();
uint64_t goblinSdFreeBytes();
const char* goblinSdCardName();
bool goblinStorageReady();
bool goblinStorageLow();
bool goblinLoggingEnabled();
bool goblinSummariesEnabled();
bool goblinChecksumEnabled();
bool goblinRetentionEnabled();
void goblinToggleLogging();
void goblinToggleSummaries();
void goblinToggleChecksum();
void goblinToggleRetention();
uint32_t goblinSummaryIntervalMs();
size_t goblinHistoryCount();
const EventRecord* goblinHistoryNewest(size_t offset);
uint32_t goblinSessionOriginMs();
bool goblinRetryStorage();
bool goblinExportSession();
bool goblinWriteDiagnostics();
const char* goblinLastFileAction();
