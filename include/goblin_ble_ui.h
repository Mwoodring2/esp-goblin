#pragma once
#include "goblin_ui.h"
#include "guard_analyzer.h"
bool goblinBlueActive();
void goblinBlueShow(TFT_eSPI& tft);
void goblinBlueShowAlert(TFT_eSPI& tft, const GuardAlert& alert);
GoblinUiAction goblinBlueTouch(TFT_eSPI& tft, int16_t x, int16_t y);
void goblinBlueRefresh(TFT_eSPI& tft);
void goblinBlueTick(TFT_eSPI& tft);
