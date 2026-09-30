#include "goblin_ui.h"
#include "es3c28p_board.h"
#include <esp_heap_caps.h>

#ifndef ESP_GOBLIN_VERSION
#define ESP_GOBLIN_VERSION "dev"
#endif

namespace {
void header(TFT_eSPI& tft, const char* right) {
    tft.fillRect(0, 0, tft.width(), 32, TFT_DARKGREEN);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREEN);
    tft.setTextSize(2);
    tft.drawString("ESP GOBLIN", 8, 16);

    tft.setTextDatum(MR_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_DARKGREEN);
    tft.drawString(right, tft.width() - 7, 16);
}

void row(TFT_eSPI& tft, int y, const char* label, const String& value, uint16_t color=TFT_LIGHTGREY) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(label, 10, y);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(value, tft.width()-10, y);
}
}

void drawBoot(TFT_eSPI& tft, bool touchOk) {
    tft.fillScreen(TFT_BLACK);
    header(tft, ESP_GOBLIN_VERSION);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("WAKING THE GOBLIN...", tft.width()/2, 85);

    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("REFERENCE BOARD A", tft.width()/2, 120);
    tft.drawString("HOSYOND ES3C28P", tft.width()/2, 136);

    tft.setTextColor(touchOk ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(touchOk ? "TOUCH FOUND" : "TOUCH NOT FOUND", tft.width()/2, 170);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Goblin sprite pack plugs in next.", tft.width()/2, 210);
}

void drawHardwareScreen(TFT_eSPI& tft, bool touchOk) {
    tft.fillScreen(TFT_BLACK);
    header(tft, "HARDWARE");

    row(tft, 50, "Board", "ES3C28P", TFT_GREEN);
    row(tft, 68, "Display", "ILI9341V 320x240", TFT_GREEN);
    row(tft, 86, "Touch", touchOk ? "FT6336G OK" : "NOT FOUND", touchOk ? TFT_GREEN : TFT_RED);
    row(tft, 104, "Flash", String(ESP.getFlashChipSize() / (1024*1024)) + " MiB");
    row(tft, 122, "PSRAM", String(ESP.getPsramSize() / (1024*1024)) + " MiB");
    row(tft, 140, "Free PSRAM", String(ESP.getFreePsram() / 1024) + " KiB");
    row(tft, 158, "Free Heap", String(ESP.getFreeHeap() / 1024) + " KiB");
    row(tft, 176, "CPU", String(ESP.getCpuFreqMHz()) + " MHz");
    row(tft, 194, "Wi-Fi", "2.4 GHz", TFT_YELLOW);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Tap screen to run AIR SNIFFER", tft.width()/2, 224);
}

void drawAirScreen(TFT_eSPI& tft, const WifiSnapshot& s) {
    tft.fillScreen(TFT_BLACK);
    header(tft, "AIR SNIFFER");

    row(tft, 45, "Access points", String(s.accessPoints), TFT_GREEN);
    row(tft, 61, "Open networks", String(s.openNetworks), s.openNetworks ? TFT_YELLOW : TFT_GREEN);
    row(tft, 77, "Strongest", String(s.strongestRssi) + " dBm");
    row(tft, 93, "Busiest channel", String(s.busiestChannel));
    row(tft, 109, "APs on busiest", String(s.busiestCount));

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("GOBLIN GUARD", tft.width()/2, 132);
    row(tft, 150, "Inventory total", String(static_cast<unsigned>(s.inventoryCount)), TFT_GREEN);
    row(tft, 168, "Known APs", String(static_cast<unsigned>(s.knownCount)), TFT_GREEN);
    row(tft, 186, "Unknown APs", String(static_cast<unsigned>(s.unknownCount)), TFT_YELLOW);
    row(tft, 204, "New this scan", String(static_cast<unsigned>(s.newCount)), TFT_YELLOW);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(s.scanFailed ? "Scan failed - tap to retry" :
        (s.inventoryIncomplete ? "Inventory incomplete - see serial" : "Tap to rescan"), tft.width()/2, 232);
}

void drawTouchMarker(TFT_eSPI& tft, int16_t x, int16_t y) {
    tft.drawCircle(x, y, 5, TFT_MAGENTA);
}
