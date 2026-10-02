#pragma once
#include "guard_analyzer.h"

void goblinGuardBegin();
void goblinGuardAnalyze(bool include_new_events = true);
bool trustAccessPoint(const uint8_t* bssid);
bool untrustAccessPoint(const uint8_t* bssid);
bool isTrustedAccessPoint(const uint8_t* bssid);
bool configureTrustedAccessPoint(const uint8_t* bssid, const char* friendly_name, bool enabled);
const TrustedApStore& goblinTrustedStore();
GuardAnalyzer& goblinGuardAlerts();
void guardFormatBssid(const uint8_t* bssid, char* output, size_t size);
