#pragma once

#include <TFT_eSPI.h>
#include "board_profile.h"
#include "security_snapshot.h"

void drawBootScreen(TFT_eSPI& tft, const BoardProfile& board);
void drawDashboard(TFT_eSPI& tft, const BoardProfile& board, const WifiSnapshot& snapshot);
