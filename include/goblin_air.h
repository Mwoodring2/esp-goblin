#pragma once
#include "air_monitor.h"
#include "wifi_promiscuous_monitor.h"

void goblinAirTick();
const AirMonitor& goblinAirMonitor();
uint32_t goblinAirMinHeap();
uint32_t goblinAirMinPsram();
