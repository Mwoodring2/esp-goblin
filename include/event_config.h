#pragma once
#include <stddef.h>
#include <stdint.h>

// Phase 5 limits. Logging bandwidth is low; bounds matter more than throughput.
static const uint8_t kEventSchemaVersion = 1;
static const size_t kEventQueueCapacity = 64;
static const size_t kHistoryCapacity = 100;
static const size_t kJsonLineCapacity = 3072;
static const size_t kDedupSlots = 48;
static const size_t kDedupIdentityLength = 48;
static const uint32_t kLogFlushIntervalMs = 5000;
static const uint32_t kLogFlushEventCount = 8;
static const uint32_t kDedupCooldownMs = 60000;
static const uint32_t kSummaryIntervalMs = 300000;
static const uint32_t kMaxSessionFiles = 100;
static const uint32_t kLowHeapBytes = 48u * 1024u;
static const uint64_t kSdFreeReserveBytes = 64ull * 1024ull * 1024ull;
static const uint8_t kSdLowSpacePercent = 10;
static const int kSdBusFrequencyKhz = 20000;
static const int kSdDataBits = 1;
static const uint64_t kWallClockMinUnixMs = 1577836800000ull; // 2020-01-01 UTC
static const uint64_t kWallClockMaxUnixMs = 4102444800000ull; // 2100-01-01 UTC

static const uint16_t kFlagTimeValid = 1u;
static const uint16_t kFlagRssi = 2u;
static const uint16_t kFlagChannel = 4u;
static const uint16_t kFlagTrusted = 8u;
static const uint16_t kFlagTrustedYes = 16u;
static const uint16_t kFlagCount = 32u;
static const uint16_t kFlagTruncated = 64u;
static const uint16_t kFlagLowerBound = 128u;
static const uint16_t kFlagReason = 256u;
