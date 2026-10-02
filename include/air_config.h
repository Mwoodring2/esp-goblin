#pragma once
#include <stdint.h>
constexpr uint32_t kChannelDwellMs = 300;
constexpr uint32_t kAirUiRefreshMs = 750;
constexpr uint32_t kAirDiagnosticsMs = 5000;
constexpr uint32_t kDisconnectBurstWindowMs = 2000;
constexpr uint32_t kDisconnectBurstThreshold = 10;
constexpr uint32_t kDisconnectAlertCooldownMs = 10000;
constexpr bool kAirVerbose = false;
