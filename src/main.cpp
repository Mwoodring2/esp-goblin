#include "goblin_ble.h"
#include "goblin_events.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "es3c28p_board.h"
#include "touch_ft6336.h"
#include "wifi_snapshot.h"
#include "goblin_ui.h"
#include "goblin_guard.h"
#include "goblin_air.h"

TFT_eSPI tft;
static bool touchOk = false;
static bool showingAir = false;
static uint32_t lastTouchMs = 0;
static bool touchHeld = false;

static void serialHardwareReport() {
    Serial.println();
    Serial.println("=== ESP GOBLIN HARDWARE REPORT ===");
    Serial.printf("Board: %s\n", GoblinBoard::NAME);
    Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
    Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("CPU: %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Touch 0x%02X: %s\n", GoblinBoard::TOUCH_ADDR, touchOk ? "FOUND" : "NOT FOUND");
    Serial.println("==================================");
    Serial.println();
}

static void rescan() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("SNIFFING THE AIR...", tft.width()/2, tft.height()/2);

    const WifiSnapshot s = goblinScanWifi();
    goblinUiShowScan(tft, s);

    Serial.printf("[wifi] aps=%d open=%d strongest=%d channel=%d count=%d\n",
        s.accessPoints, s.openNetworks, s.strongestRssi, s.busiestChannel, s.busiestCount);
}

void setup() {
    Serial.begin(115200);
    delay(400);
    goblinGuardBegin();
    goblinEventsBegin();

    pinMode(GoblinBoard::LCD_BL, OUTPUT);
    digitalWrite(GoblinBoard::LCD_BL, HIGH);

    tft.init();
    tft.setRotation(GoblinBoard::ROTATION);
    tft.fillScreen(TFT_BLACK);

    touchOk = goblinTouchBegin();
    drawBoot(tft, touchOk);
    serialHardwareReport();

    delay(1600);
    drawHardwareScreen(tft, touchOk);
}

void loop() {
    goblinBleTick();
    goblinAirTick();
    GoblinTouchPoint p;
    const bool touchRead = touchOk && goblinTouchRead(p);
    if (touchRead && p.pressed) {
        const uint32_t now = millis();

        if (!touchHeld && now - lastTouchMs > 200) {
            lastTouchMs = now;

            if (!showingAir) {
                showingAir = true;
                rescan();
            } else {
                const auto action = goblinUiTouch(tft, p.x, p.y);
                if (action == GoblinUiAction::Scan) rescan();
                else if (action == GoblinUiAction::StartMonitor) { goblinRadio().start(goblinRadioNow()); goblinUiRefresh(tft); }
                else if (action == GoblinUiAction::StopMonitor) { goblinRadio().stop(goblinRadioNow()); goblinUiRefresh(tft); }
            }
        }
        touchHeld = true;
    } else if (touchRead) touchHeld = false;

    if (showingAir) goblinUiTick(tft, touchHeld);
    goblinBleTick();
    goblinAirTick();
    goblinEventsTick();
    delay(goblinWifiMonitor().running() ? 2 : 15);
}
