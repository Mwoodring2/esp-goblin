#include "goblin_ble_ui.h"
#include "goblin_ble.h"
#include "goblin_events.h"
#include "goblin_ui.h"
#include "es3c28p_board.h"
#include "log_policy.h"
#include <esp_heap_caps.h>
#include "goblin_guard.h"
#include "goblin_air.h"
#include <esp_timer.h>
#include <cstring>

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
    const char* storage = goblinStorageLabel();
    uint16_t storage_color = TFT_GREEN;
    if (storage[3] == 'E' || storage[3] == '-') storage_color = storage[3] == 'E' ? TFT_RED : TFT_DARKGREY;
    else if (storage[3] == 'L') storage_color = TFT_YELLOW;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(storage_color, TFT_DARKGREEN);
    tft.drawString(storage, tft.width() / 2, 16);
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
    row(tft, 212, "Storage", goblinStorageLabel(), goblinStorageReady() ? TFT_GREEN : TFT_YELLOW);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Tap screen to run AIR SNIFFER", tft.width()/2, 230);
}

void drawAirScreen(TFT_eSPI& tft, const WifiSnapshot& s) {
    tft.fillScreen(TFT_BLACK);
    header(tft, "AIR >");

    const auto& inventory = goblinApInventory();
    row(tft, 48, "Inventory", String(static_cast<unsigned>(inventory.totalCount())), TFT_GREEN);
    row(tft, 72, "Known", String(static_cast<unsigned>(inventory.knownCount())), TFT_GREEN);
    row(tft, 96, "Unknown", String(static_cast<unsigned>(inventory.unknownCount())), TFT_YELLOW);
    row(tft, 120, "New this scan", String(static_cast<unsigned>(s.newCount)), TFT_YELLOW);
    row(tft, 144, "Alerts", String(static_cast<unsigned>(goblinGuardAlerts().activeCount())) +
        (goblinGuardAlerts().droppedCount() ? "+ (overflow)" : ""), TFT_YELLOW);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("AIR SNIFFER: " + String(s.accessPoints) + " APs / " + String(s.openNetworks) + " open",
        tft.width()/2, 167);
    tft.drawString("Strong " + String(s.strongestRssi) + " dBm / Ch " + String(s.busiestChannel) +
        " (" + String(s.busiestCount) + ")", tft.width()/2, 181);
    if (!goblinTrustedStore().ready() || s.scanFailed || s.inventoryIncomplete || s.monitorRestoreFailed) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString(s.monitorRestoreFailed ? "Monitor restore failed - use AIR" : !goblinTrustedStore().ready() ? goblinTrustedStore().errorText() :
            (s.scanFailed ? "Scan failed - tap RESCAN" : "Inventory incomplete - see serial"), tft.width()/2, 195);
    }
}

void drawTouchMarker(TFT_eSPI& tft, int16_t x, int16_t y) {
    tft.drawCircle(x, y, 5, TFT_MAGENTA);
}

namespace {
enum class GuardScreen { Summary, List, Detail, Alert, Air, Channels, BurstDetail, Storage, History, EventDetail, LogOptions };
GuardScreen screen = GuardScreen::Summary;
WifiSnapshot lastSnapshot;
size_t page = 0;
size_t alertIndex = 0;
uint8_t selected[6] = {};
String notice;
constexpr int ButtonY = 205;
bool returnToAir = false;
uint32_t lastAirRefresh = 0, lastAirNotification = 0;
uint32_t lastBleNotification = 0;
size_t history_offset = 0;
size_t history_selected = 0;
int option_index = 0;
GuardAlert displayedAlert;

String displayText(const char* value, size_t limit = 32) {
    String text(value && value[0] ? value : "Hidden");
    for (unsigned i = 0; i < text.length(); ++i)
        if (static_cast<uint8_t>(text[i]) < 32 || static_cast<uint8_t>(text[i]) == 127) text.setCharAt(i, '?');
    if (text.length() > limit) text = text.substring(0, limit - 3) + "...";
    return text;
}
String address(const uint8_t* bssid) {
    char buffer[18]; guardFormatBssid(bssid, buffer, sizeof(buffer)); return String(buffer);
}
void line(TFT_eSPI& tft, int y, const String& text, uint16_t color = TFT_LIGHTGREY) {
    tft.setTextSize(1); tft.setTextDatum(TL_DATUM); tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text, 10, y);
}
void button(TFT_eSPI& tft, int slot, const char* label) {
    const int x = 4 + slot * 106;
    tft.drawRoundRect(x, ButtonY, 100, 32, 4, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(label, x + 50, ButtonY + 16);
}
void summary(TFT_eSPI& tft) {
    screen = GuardScreen::Summary;
    drawAirScreen(tft, lastSnapshot);
    button(tft, 0, "VIEW APs"); button(tft, 1, "RESCAN"); button(tft, 2, "ALERTS");
}
String number(uint64_t value) {
    char text[24]; snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(value)); return String(text);
}
void airScreen(TFT_eSPI& tft) {
    screen = GuardScreen::Air;
    tft.fillScreen(TFT_BLACK); header(tft, "BLUE >");
    const auto& radio = goblinWifiMonitor();
    const auto& total = goblinAirMonitor().lifetime();
    const auto recent = goblinAirMonitor().rolling(static_cast<uint64_t>(esp_timer_get_time()));
    line(tft, 39, radio.failed() ? "ERROR - see serial" : radio.healthy() ? "MONITORING" : goblinRadio().enabled() ? "PAUSED / BLE" : "STOPPED",
        radio.failed() ? TFT_RED : TFT_GREEN);
    tft.setTextDatum(TR_DATUM); tft.drawString("CH " + String(radio.channel()) + " >", 310, 39);
    row(tft, 66, "Mgmt/sec (~1s)", number(recent.management));
    row(tft, 84, "Beacons", number(total.beacons));
    row(tft, 102, "Probes req / resp", number(total.probe_requests) + " / " + number(total.probe_responses));
    row(tft, 120, "Authentication", number(total.authentication));
    row(tft, 138, "Deauth", number(total.deauthentication), TFT_YELLOW);
    row(tft, 156, "Disassoc", number(total.disassociation), TFT_YELLOW);
    row(tft, 174, "Queue / high-water", String(goblinWifiMonitor().queue().queued()) + " / " + String(goblinWifiMonitor().queue().highWater()));
    row(tft, 192, "Dropped", number(total.queue_drops), total.queue_drops ? TFT_ORANGE : TFT_GREEN);
    if (lastSnapshot.scanFailed || lastSnapshot.monitorRestoreFailed) {
        tft.fillRect(0, 52, 320, 9, TFT_BLACK);
        line(tft, 52, "Last AP scan/restore failed - see serial", TFT_RED);
    }
    button(tft, 0, "AP SCAN");
    const String alerts = "ALERTS " + String(static_cast<unsigned>(goblinGuardAlerts().activeCount()));
    button(tft, 1, alerts.c_str()); button(tft, 2, goblinRadio().enabled() ? "STOP" : "START");
}
void channelsScreen(TFT_eSPI& tft) {
    screen = GuardScreen::Channels;
    tft.fillScreen(TFT_BLACK); header(tft, "AIR <");
    line(tft, 39, "Packet activity - not RF energy", TFT_YELLOW);
    const auto& total = goblinAirMonitor().lifetime();
    auto& radio = goblinWifiMonitor();
    uint64_t maximum = 1;
    for (int ch = 1; ch <= 14; ++ch) if (total.channel[ch] > maximum) maximum = total.channel[ch];
    for (unsigned i = 0; i < radio.channelCount(); ++i) {
        const unsigned ch = radio.firstChannel() + i;
        const int x = 8 + (i / 7) * 158, y = 59 + (i % 7) * 18;
        tft.setTextDatum(TL_DATUM); tft.setTextColor(ch == radio.channel() ? TFT_GREEN : TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString(String(ch) + ": " + number(total.channel[ch]), x, y);
        const unsigned width = static_cast<unsigned>((static_cast<double>(total.channel[ch]) / maximum) * 42);
        tft.fillRect(x + 107, y, width, 8, TFT_DARKGREEN);
    }
    if (!radio.channelCount()) line(tft, 72, "Start monitor to read country channels");
    line(tft, 188, "Total management: " + number(total.management));
    button(tft, 0, "AIR"); button(tft, 1, "GUARD"); button(tft, 2, goblinRadio().enabled() ? "STOP" : "START");
}
void list(TFT_eSPI& tft) {
    screen = GuardScreen::List;
    tft.fillScreen(TFT_BLACK); header(tft, "AP LIST");
    const size_t count = goblinApInventory().totalCount();
    const size_t pages = count ? (count + 2) / 3 : 1;
    if (page >= pages) page = pages - 1;
    for (size_t i = 0; i < 3; ++i) {
        const auto* ap = goblinApInventory().at(page * 3 + i);
        if (!ap) break;
        const int y = 39 + i * 51;
        tft.drawRect(4, y - 2, 312, 49, TFT_DARKGREY);
        const auto* trusted = goblinTrustedStore().find(ap->bssid);
        line(tft, y, displayText(trusted && trusted->friendly_name[0] ? trusted->friendly_name : ap->ssid));
        line(tft, y + 14, address(ap->bssid), TFT_DARKGREY);
        line(tft, y + 28, String(ap->rssi) + " dBm   " +
            (ap->state == AccessPointState::Known ? "KNOWN" : "UNKNOWN"),
            ap->state == AccessPointState::Known ? TFT_GREEN : TFT_YELLOW);
    }
    if (!count) line(tft, 74, "No APs yet. Rescan from Guard.");
    line(tft, 193, "Page " + String(static_cast<unsigned>(page + 1)) + "/" + String(static_cast<unsigned>(pages)));
    button(tft, 0, "BACK"); button(tft, 1, "PREV"); button(tft, 2, "NEXT");
}
void detail(TFT_eSPI& tft) {
    screen = GuardScreen::Detail;
    tft.fillScreen(TFT_BLACK); header(tft, "AP DETAIL");
    const auto* ap = goblinApInventory().find(selected);
    if (!ap) { summary(tft); return; }
    line(tft, 40, "SSID: " + displayText(ap->ssid));
    line(tft, 59, "BSSID: " + address(ap->bssid));
    line(tft, 78, "RSSI: " + String(ap->rssi) + " dBm");
    line(tft, 97, "Channel: " + String(ap->channel));
    line(tft, 116, "Security: " + String(guardSecurityName(ap->auth_mode)) + " (" + String(ap->auth_mode) + ")");
    line(tft, 135, "Seen count: " + String(ap->seen_count));
    line(tft, 154, ap->state == AccessPointState::Known ? "KNOWN" : "UNKNOWN",
        ap->state == AccessPointState::Known ? TFT_GREEN : TFT_YELLOW);
    line(tft, 173, goblinApInventory().seenThisScan(ap->bssid) ? "Observed in latest scan" : "Not seen in latest scan", TFT_DARKGREY);
    if (notice.length()) line(tft, 191, notice, TFT_YELLOW);
    button(tft, 0, "BACK");
    // The trust action spans two slots for a reliable touch target.
    tft.drawRoundRect(110, ButtonY, 206, 32, 4, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(ap->state == AccessPointState::Known ? "FORGET AP" : "TRUST THIS AP", 213, ButtonY + 16);
}
void alertScreen(TFT_eSPI& tft) {
    const auto* alert = goblinGuardAlerts().at(alertIndex);
    if (!alert) { summary(tft); return; }
    if (guardBleAlert(alert->type)) { goblinBlueShowAlert(tft, *alert); return; }
    displayedAlert = *alert;
    screen = GuardScreen::Alert;
    tft.fillScreen(TFT_BLACK); header(tft, "ALERT");
    line(tft, 39, guardAlertName(alert->type), guardImportant(alert->type) ? TFT_ORANGE : TFT_YELLOW);
    line(tft, 56, alert->type == GuardAlertType::DisconnectBurst && !alert->wifi.ssid[0] ?
        String("AP name unavailable") : displayText(alert->wifi.ssid));
    if (alert->type == GuardAlertType::DisconnectBurst) {
        line(tft, 75, "BSSID: " + address(alert->wifi.bssid));
        line(tft, 94, "Frames: " + String(alert->wifi.frame_count) + (alert->wifi.count_lower_bound ? "+" : "") +
            " / " + String(alert->wifi.window_ms / 1000) + " sec");
        line(tft, 115, String("Trusted AP: ") + (alert->wifi.trusted_ap ? "YES" : "NO"), alert->wifi.trusted_ap ? TFT_ORANGE : TFT_YELLOW);
        line(tft, 156, "Observation, not proof of malicious intent.", TFT_YELLOW);
    } else if (alert->type == GuardAlertType::SsidCollision) {
        line(tft, 75, "Known: " + address(alert->wifi.trusted_bssid));
        line(tft, 94, "Observed: " + address(alert->wifi.bssid));
        line(tft, 118, "Security: " + String(guardSecurityName(alert->wifi.auth_mode)) + " (" + String(alert->wifi.auth_mode) + ")");
        line(tft, 156, "SSID match is not proof of an attack.", TFT_YELLOW);
    } else if (alert->type == GuardAlertType::NewAccessPoint) {
        line(tft, 75, address(alert->wifi.bssid));
        line(tft, 96, "Security: " + String(guardSecurityName(alert->wifi.auth_mode)) + " (" + String(alert->wifi.auth_mode) + ")");
        line(tft, 156, "First observation this boot.", TFT_DARKGREY);
    } else {
        line(tft, 75, address(alert->wifi.bssid));
        line(tft, 96, "Old: " + displayText(alert->wifi.old_value));
        line(tft, 115, "New: " + displayText(alert->wifi.new_value));
        if (alert->type == GuardAlertType::TrustedChannelChanged)
            line(tft, 156, "Informational: routers can change channel.", TFT_YELLOW);
    }
    line(tft, 137, "RSSI: " + String(alert->wifi.rssi) + " dBm / Channel: " + String(alert->wifi.channel));
    line(tft, 181, "Alert " + String(static_cast<unsigned>(alertIndex + 1)) + "/" +
        String(static_cast<unsigned>(goblinGuardAlerts().count())));
    button(tft, 0, "DETAILS"); button(tft, 1, "DISMISS"); button(tft, 2, "NEXT/BACK");
}
void burstDetail(TFT_eSPI& tft) {
    screen = GuardScreen::BurstDetail;
    tft.fillScreen(TFT_BLACK); header(tft, "DISCONNECT");
    const auto& a = displayedAlert;
    line(tft, 40, "BSSID: " + address(a.wifi.bssid));
    line(tft, 58, "Last source: " + address(a.wifi.source));
    line(tft, 76, "Channel: " + String(a.wifi.channel) + " / RSSI: " + String(a.wifi.rssi));
    line(tft, 94, "Frames: " + String(a.wifi.frame_count) + (a.wifi.count_lower_bound ? "+" : "") + " / " + String(a.wifi.window_ms) + " ms");
    line(tft, 112, "Last reason: " + (a.wifi.has_reason_code ? String(a.wifi.reason_code) : String("unavailable/protected")));
    line(tft, 130, String("Trusted BSSID: ") + (a.wifi.trusted_ap ? "YES" : "NO"));
    line(tft, 148, String("Source equals BSSID: ") + (!std::memcmp(a.wifi.source, a.wifi.bssid, 6) ? "YES" : "NO"));
    line(tft, 168, "Addresses do not authenticate a sender.", TFT_YELLOW);
    line(tft, 187, "No malicious intent inferred.", TFT_YELLOW);
    button(tft, 0, "BACK"); button(tft, 1, "AP SCAN"); button(tft, 2, "DISMISS");
}
void storageScreen(TFT_eSPI& tft) {
    screen = GuardScreen::Storage;
    tft.fillScreen(TFT_BLACK); header(tft, "BACK");
    char total[16], free_bytes[16];
    formatByteCount(goblinSdTotalBytes(), total, sizeof(total));
    formatByteCount(goblinSdFreeBytes(), free_bytes, sizeof(free_bytes));
    row(tft, 42, "Card", goblinSdCardName(), goblinStorageReady() ? TFT_GREEN : TFT_YELLOW);
    row(tft, 60, "Size", goblinStorageReady() ? total : "--");
    row(tft, 78, "Free", goblinStorageReady() ? free_bytes : "--", goblinStorageLow() ? TFT_YELLOW : TFT_LIGHTGREY);
    row(tft, 96, "Session", String(goblinSessionId()));
    row(tft, 114, "Events", String(goblinEventsWritten()));
    row(tft, 132, "Dropped", String(goblinEventsDropped()), goblinEventsDropped() ? TFT_ORANGE : TFT_GREEN);
    row(tft, 150, "Logging", String(goblinLoggingEnabled() ? "ON" : "OFF") + "  >");
    line(tft, 168, goblinLastFileAction(), TFT_DARKGREY);
    line(tft, 186, "Observations only. No intent inferred.", TFT_YELLOW);
    button(tft, 0, "HISTORY");
    button(tft, 1, goblinStorageReady() ? "EXPORT" : "RETRY SD");
    button(tft, 2, "DIAG");
}
void historyScreen(TFT_eSPI& tft) {
    screen = GuardScreen::History;
    tft.fillScreen(TFT_BLACK); header(tft, "BACK");
    const size_t count = goblinHistoryCount();
    if (history_offset >= count && count) history_offset = count - 1;
    if (history_selected < history_offset) history_selected = history_offset;
    if (history_selected >= history_offset + 5) history_selected = history_offset;
    for (size_t row_index = 0; row_index < 5; ++row_index) {
        const EventRecord* event = goblinHistoryNewest(history_offset + row_index);
        const int y = 40 + static_cast<int>(row_index) * 30;
        if (!event) break;
        if (history_offset + row_index == history_selected) tft.drawRect(4, y - 3, 312, 28, TFT_DARKGREEN);
        char when[20];
        const uint32_t origin = event->session_id == goblinSessionId() ? goblinSessionOriginMs() : 0;
        formatEvidenceTime(event->flags & kFlagTimeValid, event->unix_ms, event->monotonic_ms, origin, when, sizeof(when));
        line(tft, y, String(when) + "  " + eventShortName(event->type),
             event->severity == EventSeverity::Warning || event->severity == EventSeverity::Critical ? TFT_ORANGE : TFT_LIGHTGREY);
    }
    if (!count) line(tft, 80, "No events yet");
    button(tft, 0, "UP"); button(tft, 1, "OPEN"); button(tft, 2, "DOWN");
}
void eventDetailScreen(TFT_eSPI& tft) {
    screen = GuardScreen::EventDetail;
    const EventRecord* event = goblinHistoryNewest(history_selected);
    if (!event) { historyScreen(tft); return; }
    tft.fillScreen(TFT_BLACK); header(tft, "BACK");
    line(tft, 38, eventShortName(event->type), TFT_YELLOW);
    line(tft, 54, "Session: " + String(event->session_id));
    char when[20];
    const uint32_t origin = event->session_id == goblinSessionId() ? goblinSessionOriginMs() : 0;
    formatEvidenceTime(event->flags & kFlagTimeValid, event->unix_ms, event->monotonic_ms, origin, when, sizeof(when));
    line(tft, 70, String("Time: ") + when);
    int y = 88;
    if (event->summary[0]) { line(tft, y, displayText(event->summary, 42)); y += 16; }
    if (event->ssid[0]) { line(tft, y, "SSID: " + displayText(event->ssid)); y += 16; }
    if (event->bssid[0]) { line(tft, y, "BSSID: " + String(event->bssid)); y += 16; }
    if (event->trusted_bssid[0]) { line(tft, y, "Trusted: " + String(event->trusted_bssid)); y += 16; }
    if (event->ble_address[0]) { line(tft, y, "BLE: " + String(event->ble_address)); y += 16; }
    if (event->ble_name[0]) { line(tft, y, "Name: " + displayText(event->ble_name)); y += 16; }
    if (event->flags & kFlagChannel) { line(tft, y, "Channel: " + String(event->channel)); y += 16; }
    if (event->flags & kFlagRssi) { line(tft, y, "RSSI: " + String(event->rssi)); y += 16; }
    if (event->flags & kFlagCount) {
        line(tft, y, "Frames: " + String(event->count) + ((event->flags & kFlagLowerBound) ? "+" : "") +
            (event->span_ms ? " / " + String(event->span_ms) + " ms" : ""));
        y += 16;
    }
    if (event->flags & kFlagTrusted) { line(tft, y, String("Trusted AP: ") + ((event->flags & kFlagTrustedYes) ? "YES" : "NO")); y += 16; }
    if (event->old_value[0] || event->new_value[0]) {
        line(tft, y, "Old: " + displayText(event->old_value)); y += 16;
        line(tft, y, "New: " + displayText(event->new_value));
    }
    button(tft, 0, "BACK");
}
void logOptionsScreen(TFT_eSPI& tft) {
    screen = GuardScreen::LogOptions;
    tft.fillScreen(TFT_BLACK); header(tft, "BACK");
    const char* labels[] = {"Event logging", "Periodic summaries", "Integrity checksum", "Retention"};
    const char* values[] = {goblinLoggingEnabled() ? "ON" : "OFF", goblinSummariesEnabled() ? "ON" : "OFF",
                            goblinChecksumEnabled() ? "ON" : "OFF", goblinRetentionEnabled() ? "AUTO" : "OFF"};
    for (int i = 0; i < 4; ++i) {
        if (i == option_index) tft.drawRect(6, 42 + i * 28, 308, 24, TFT_DARKGREEN);
        line(tft, 48 + i * 28, String(labels[i]) + ": " + values[i]);
    }
    line(tft, 168, "Summary interval: 5 min", TFT_DARKGREY);
    line(tft, 186, "SD mode: 1-bit", TFT_DARKGREY);
    button(tft, 0, "BACK"); button(tft, 1, "NEXT"); button(tft, 2, "TOGGLE");
}
bool locateDisplayedAlert() {
    for (size_t i = 0; i < goblinGuardAlerts().count(); ++i) {
        const auto* a = goblinGuardAlerts().at(i);
        if (a->type == displayedAlert.type && !std::memcmp(a->wifi.bssid, displayedAlert.wifi.bssid, 6) &&
            !std::memcmp(a->wifi.trusted_bssid, displayedAlert.wifi.trusted_bssid, 6)) { alertIndex = i; return true; }
    }
    return false;
}
int firstActive() {
    for (size_t i = 0; i < goblinGuardAlerts().count(); ++i)
        if (!goblinGuardAlerts().at(i)->dismissed) return static_cast<int>(i);
    return -1;
}
}

void goblinUiShowScan(TFT_eSPI& tft, const WifiSnapshot& snapshot) {
    lastSnapshot = snapshot;
    const int important = goblinGuardAlerts().firstImportant();
    if (important >= 0) { alertIndex = important; alertScreen(tft); }
    else if (returnToAir) airScreen(tft);
    else summary(tft);
    returnToAir = false;
}

GoblinUiAction goblinUiTouch(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (goblinBlueActive()) { const auto action = goblinBlueTouch(tft, x, y); if (!goblinBlueActive()) airScreen(tft); return action; }
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return GoblinUiAction::None;
    if (y < 32 && x >= 210) {
        if (screen == GuardScreen::Storage) { summary(tft); return GoblinUiAction::None; }
        if (screen == GuardScreen::History || screen == GuardScreen::EventDetail || screen == GuardScreen::LogOptions) {
            storageScreen(tft); return GoblinUiAction::None;
        }
        if (screen == GuardScreen::Summary || screen == GuardScreen::Channels) airScreen(tft);
        else if (screen == GuardScreen::Air) goblinBlueShow(tft);
        return GoblinUiAction::None;
    }
    if (y < 32 && x >= 100 && x < 210 &&
        (screen == GuardScreen::Summary || screen == GuardScreen::Air || screen == GuardScreen::Channels)) {
        storageScreen(tft); return GoblinUiAction::None;
    }
    if (screen == GuardScreen::Air && y >= 32 && y < 57 && x >= 215) {
        channelsScreen(tft); return GoblinUiAction::None;
    }
    if (screen == GuardScreen::List && y >= 37 && y < 190) {
        const auto* ap = goblinApInventory().at(page * 3 + static_cast<size_t>((y - 37) / 51));
        if (ap) { std::memcpy(selected, ap->bssid, 6); notice = ""; detail(tft); }
        return GoblinUiAction::None;
    }
    if (screen == GuardScreen::History && y >= 37 && y < 190) {
        const size_t row_index = static_cast<size_t>((y - 37) / 30);
        if (history_offset + row_index < goblinHistoryCount()) {
            history_selected = history_offset + row_index;
            historyScreen(tft);
        }
        return GoblinUiAction::None;
    }
    if (screen == GuardScreen::Storage && y >= 145 && y < 168) {
        logOptionsScreen(tft);
        return GoblinUiAction::None;
    }
    if (y < ButtonY || x < 4 || x >= 316 || y >= 237) return GoblinUiAction::None;
    const int slot = (screen == GuardScreen::Detail && x >= 110) ? 1 :
        (x <= 103 ? 0 : (x >= 110 && x <= 209 ? 1 : (x >= 216 ? 2 : -1)));
    if (slot < 0) return GoblinUiAction::None;
    switch (screen) {
        case GuardScreen::Air:
            if (slot == 0) { returnToAir = true; return GoblinUiAction::Scan; }
            if (slot == 1) { const int active = firstActive(); if (active >= 0) { alertIndex = active; alertScreen(tft); } }
            else return goblinRadio().enabled() ? GoblinUiAction::StopMonitor : GoblinUiAction::StartMonitor;
            break;
        case GuardScreen::Channels:
            if (slot == 0) airScreen(tft);
            else if (slot == 1) summary(tft);
            else return goblinRadio().enabled() ? GoblinUiAction::StopMonitor : GoblinUiAction::StartMonitor;
            break;
        case GuardScreen::BurstDetail:
            if (!locateDisplayedAlert()) { summary(tft); break; }
            if (slot == 0) alertScreen(tft);
            else if (slot == 1) { returnToAir = true; return GoblinUiAction::Scan; }
            else { goblinLogDismiss(displayedAlert); goblinGuardAlerts().dismiss(alertIndex); airScreen(tft); }
            break;
        case GuardScreen::Summary:
            if (slot == 0) { page = 0; list(tft); }
            else if (slot == 1) return GoblinUiAction::Scan;
            else { const int active = firstActive(); if (active >= 0) { alertIndex = active; alertScreen(tft); } }
            break;
        case GuardScreen::List:
            if (slot == 0) summary(tft);
            else { if (slot == 1 && page) --page;
                   if (slot >= 2 && (page + 1) * 3 < goblinApInventory().totalCount()) ++page;
                   list(tft); }
            break;
        case GuardScreen::Detail:
            if (slot == 0) list(tft);
            else {
                const bool known = isTrustedAccessPoint(selected);
                const bool ok = known ? untrustAccessPoint(selected) : trustAccessPoint(selected);
                notice = ok ? (known ? "AP forgotten" : "Trusted baseline saved") : goblinTrustedStore().errorText();
                detail(tft);
            }
            break;
        case GuardScreen::Alert:
            if (!locateDisplayedAlert()) { summary(tft); break; }
            if (slot == 0) {
                const auto* alert = goblinGuardAlerts().at(alertIndex);
                if (alert && alert->type == GuardAlertType::DisconnectBurst) burstDetail(tft);
                else if (alert) { std::memcpy(selected, alert->wifi.bssid, 6); notice = ""; detail(tft); }
            } else if (slot == 1) {
                const auto* alert = goblinGuardAlerts().at(alertIndex);
                if (alert) goblinLogDismiss(*alert);
                goblinGuardAlerts().dismiss(alertIndex);
                const int active = firstActive();
                if (active >= 0) { alertIndex = active; alertScreen(tft); } else summary(tft);
            } else {
                size_t next = alertIndex + 1;
                while (next < goblinGuardAlerts().count() && goblinGuardAlerts().at(next)->dismissed) ++next;
                if (next < goblinGuardAlerts().count()) { alertIndex = next; alertScreen(tft); } else summary(tft);
            }
            break;
        case GuardScreen::Storage:
            if (slot == 0) { history_offset = 0; history_selected = 0; historyScreen(tft); }
            else if (slot == 1) {
                const bool ok = goblinStorageReady() ? goblinExportSession() : goblinRetryStorage();
                (void)ok;
                storageScreen(tft);
            } else { goblinWriteDiagnostics(); storageScreen(tft); }
            break;
        case GuardScreen::History:
            if (slot == 0 && history_offset) --history_offset;
            else if (slot == 2 && history_offset + 5 < goblinHistoryCount()) ++history_offset;
            if (slot != 1) history_selected = history_offset;
            if (slot == 1) eventDetailScreen(tft);
            else historyScreen(tft);
            break;
        case GuardScreen::EventDetail:
            if (slot == 0) historyScreen(tft);
            break;
        case GuardScreen::LogOptions:
            if (slot == 0) storageScreen(tft);
            else if (slot == 1) { option_index = (option_index + 1) % 4; logOptionsScreen(tft); }
            else {
                if (option_index == 0) goblinToggleLogging();
                else if (option_index == 1) goblinToggleSummaries();
                else if (option_index == 2) goblinToggleChecksum();
                else goblinToggleRetention();
                logOptionsScreen(tft);
            }
            break;
    }
    return GoblinUiAction::None;
}

void goblinUiRefresh(TFT_eSPI& tft) {
    if (goblinBlueActive()) { goblinBlueRefresh(tft); return; }
    if (screen == GuardScreen::Channels) channelsScreen(tft);
    else if (screen == GuardScreen::Air) airScreen(tft);
}
void goblinUiTick(TFT_eSPI& tft, bool touch_held) {
    if (touch_held) return;
    if (goblinBlueActive()) { goblinBlueTick(tft); return; }
    if (screen != GuardScreen::Air && screen != GuardScreen::Channels && screen != GuardScreen::Summary) return;
    if (goblinAirMonitor().notifications() != lastAirNotification || goblinBleNotifications() != lastBleNotification) {
        lastAirNotification = goblinAirMonitor().notifications();
        lastBleNotification = goblinBleNotifications();
        const int important = goblinGuardAlerts().firstImportant();
        if (important >= 0) { alertIndex = important; alertScreen(tft); return; }
    }
    if (millis() - lastAirRefresh >= kAirUiRefreshMs) {
        lastAirRefresh = millis(); goblinUiRefresh(tft);
    }
}
