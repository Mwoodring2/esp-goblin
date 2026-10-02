#pragma once
#include <TFT_eSPI.h>
#include "wifi_snapshot.h"

void drawBoot(TFT_eSPI& tft, bool touchOk);
void drawHardwareScreen(TFT_eSPI& tft, bool touchOk);
void drawAirScreen(TFT_eSPI& tft, const WifiSnapshot& s);
void drawTouchMarker(TFT_eSPI& tft, int16_t x, int16_t y);
void goblinUiShowScan(TFT_eSPI& tft, const WifiSnapshot& snapshot);
enum class GoblinUiAction { None, Scan, StartMonitor, StopMonitor };
GoblinUiAction goblinUiTouch(TFT_eSPI& tft, int16_t x, int16_t y);
void goblinUiTick(TFT_eSPI& tft, bool touch_held);
void goblinUiRefresh(TFT_eSPI& tft);
