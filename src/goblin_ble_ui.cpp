#include "goblin_ble_ui.h"
#include "goblin_events.h"
#include "goblin_ui.h"
#include "goblin_ble.h"
#include "goblin_guard.h"
#include <cstdio>
namespace {
enum class Page { Overview, Devices, Details, Hunt, Privacy };
Page page = Page::Overview;
bool active = false;
size_t list_page = 0;
unsigned detail_page = 0;
BleKey selected, visible[3];
size_t visible_count = 0;
GuardAlert selected_alert;
uint64_t refreshed = 0;
String num(uint64_t n) { char s[24]; snprintf(s, sizeof(s), "%llu", static_cast<unsigned long long>(n)); return String(s); }
String addr(const BleKey& key) { char s[18]; guardFormatBssid(key.address, s, sizeof(s)); return String(s); }
void line(TFT_eSPI& t, int y, const String& s, uint16_t color = TFT_LIGHTGREY) {
    t.setTextSize(1); t.setTextDatum(TL_DATUM); t.setTextColor(color, TFT_BLACK); t.drawString(s, 8, y);
}
void button(TFT_eSPI& t, unsigned slot, const char* s) {
    t.drawRoundRect(4 + slot * 106, 205, 100, 32, 4, TFT_BLUE);
    t.setTextSize(1); t.setTextDatum(MC_DATUM); t.setTextColor(TFT_CYAN, TFT_BLACK); t.drawString(s, 54 + slot * 106, 221);
}
void header(TFT_eSPI& t, const char* label) {
    t.fillScreen(TFT_BLACK); t.fillRect(0, 0, 320, 32, TFT_NAVY);
    t.setTextSize(2); t.setTextDatum(ML_DATUM); t.setTextColor(TFT_CYAN, TFT_NAVY); t.drawString("ESP GOBLIN", 8, 16);
    t.setTextSize(1); t.setTextDatum(MR_DATUM); t.drawString(label, 312, 16);
}
String age(uint64_t timestamp) { const uint64_t now = goblinRadioNow(); return String((now >= timestamp ? now - timestamp : 0) / 1000.0, 1) + " sec ago"; }
const GuardAlert* findAlert() {
    for (size_t i = 0; i < goblinGuardAlerts().count(); ++i) {
        const auto* a = goblinGuardAlerts().at(i);
        if (guardBleAlert(a->type) && !a->dismissed && bleSameKey(a->ble.key, selected_alert.ble.key)) return a;
    }
    return nullptr;
}
bool nextAlert() {
    for (size_t i = 0; i < goblinGuardAlerts().count(); ++i) {
        const auto* a = goblinGuardAlerts().at(i);
        if (guardBleAlert(a->type) && !a->dismissed) { selected_alert = *a; selected = a->ble.key; return true; }
    }
    selected_alert = GuardAlert{}; return false;
}
void overview(TFT_eSPI& t) {
    header(t, "BLUE / AIR <"); const auto& radio = goblinRadio();
    line(t, 39, radio.mode() == RadioMode::BlePrivacyScan ? "BLE WINDOW ACTIVE" : "BLE LISTENING PAUSED", TFT_CYAN);
    line(t, 54, radio.mode() == RadioMode::WifiMonitor ? "AIR ACTIVE / next BLE: " + String(radio.nextBleMs(goblinRadioNow()) / 1000.0, 1) + "s" : String(radioModeName(radio.mode())));
    unsigned nearby = 0, unknown = 0, beacons = 0, trackers = 0, persistent = 0;
    const BleDeviceRecord* strongest = nullptr;
    const uint64_t now = goblinRadioNow();
    for (size_t i = 0; i < goblinBleInventory().count(); ++i) {
        const auto* r = goblinBleInventory().at(i);
        if (now - r->last_seen > 60000) continue;
        ++nearby; const auto c = r->advertisement.classification;
        if (c == BleDeviceClass::Unknown || c == BleDeviceClass::GenericBle) ++unknown;
        if (c == BleDeviceClass::Beacon || c == BleDeviceClass::IBeacon || c == BleDeviceClass::Eddystone) ++beacons;
        if (c == BleDeviceClass::GoogleFindHub || c == BleDeviceClass::TrackerCandidate) ++trackers;
        if (r->persistent) ++persistent;
        if (!strongest || r->rssi > strongest->rssi) strongest = r;
    }
    line(t, 76, "Nearby (<60s): " + String(nearby) + "   Unknown: " + String(unknown));
    line(t, 95, "Beacons: " + String(beacons)); line(t, 114, "Tracker-capable: " + String(trackers), TFT_YELLOW);
    line(t, 133, "Persistent: " + String(persistent), TFT_YELLOW);
    line(t, 156, strongest ? String("Strongest: ") + bleClassName(strongest->advertisement.classification) : String("Strongest: unavailable"));
    if (strongest) line(t, 173, String(strongest->rssi) + " dBm / " + age(strongest->last_seen));
    line(t, 189, "Passive observations; identity may rotate.", TFT_DARKGREY);
    button(t, 0, "DEVICES"); button(t, 1, "PRIVACY"); button(t, 2, radio.enabled() ? "STOP" : "START");
}
void devices(TFT_eSPI& t) {
    header(t, "DEVICES"); visible_count = 0;
    const size_t count = goblinBleInventory().count(), pages = count ? (count + 2) / 3 : 1;
    if (list_page >= pages) list_page = pages - 1;
    for (size_t i = 0; i < 3; ++i) {
        const auto* r = goblinBleInventory().at(list_page * 3 + i); if (!r) break;
        visible[visible_count++] = r->key; const int y = 39 + i * 51;
        t.drawRect(4, y - 2, 312, 49, TFT_DARKGREY);
        line(t, y, bleClassName(r->advertisement.classification), TFT_CYAN);
        line(t, y + 14, addr(r->key));
        line(t, y + 28, String(r->rssi) + " dBm / " + bleConfidenceName(r->advertisement.confidence) + " / " + age(r->last_seen));
    }
    if (!count) line(t, 76, "No BLE observations. Start AIR/BLUE.");
    line(t, 193, "Page " + num(list_page + 1) + "/" + num(pages));
    button(t, 0, "BACK"); button(t, 1, "PREV"); button(t, 2, "NEXT");
}
void details(TFT_eSPI& t) {
    header(t, "DETAILS"); const auto* r = goblinBleInventory().find(selected);
    if (!r) { line(t, 55, "Observation expired / evicted"); button(t, 0, "BACK"); return; }
    const auto& a = r->advertisement;
    if (detail_page == 0) {
        line(t, 40, String(bleClassName(a.classification)) + " / " + bleConfidenceName(a.confidence), TFT_CYAN);
        line(t, 60, addr(r->key) + " type=" + String(r->key.type));
        line(t, 80, r->stability == BleIdentityStability::RandomAddress ? "Random/private address; may rotate" : r->stability == BleIdentityStability::Stable ? "Public address; not permanent identity" : "Identity stability unknown");
        line(t, 100, "RSSI " + String(r->rssi) + " / best " + String(r->strongest_rssi) + " dBm");
        line(t, 120, String("Name: ") + (a.name[0] ? a.name : "unavailable"));
        char company[32]; snprintf(company, sizeof(company), "Company ID 0x%04X", a.company);
        line(t, 140, a.has_company ? String(bleCompanyName(a.company) ? bleCompanyName(a.company) : company) : String("Manufacturer: unavailable"));
        line(t, 161, String("Rule: ") + a.signature);
        line(t, 183, "Protocol confidence is not malicious intent.", TFT_YELLOW);
    } else if (detail_page == 1) {
        line(t, 40, "First seen: " + num(r->first_seen) + " ms since boot");
        line(t, 63, "Last seen: " + age(r->last_seen));
        line(t, 86, "Last timestamp: " + num(r->last_seen) + " ms");
        line(t, 109, "Seen count: " + num(r->seen_count));
        line(t, 132, String("Persistent nearby: ") + (r->persistent ? "YES" : "NO"));
        line(t, 156, "Requires >=3 windows over >=10 min.");
        line(t, 178, "120 sec absent resets this observation.");
    } else {
        line(t, 40, "Advertised services (UUID display order):");
        for (uint8_t i = 0; i < a.service_count; ++i) {
            char text[33] = {}; const auto& s = a.services[i];
            for (uint8_t j = 0; j < s.length; ++j) snprintf(text + j * 2, sizeof(text) - j * 2, "%02X", s.bytes[s.length - 1 - j]);
            line(t, 65 + i * 24, text);
        }
        if (!a.service_count) line(t, 70, "Unavailable");
        line(t, 174, a.service_overflow ? "Additional services omitted (limit 4)" : "Bounded to 4 unique services");
        if (a.malformed) line(t, 190, "Latest payload incomplete/malformed", TFT_YELLOW);
    }
    button(t, 0, "BACK"); button(t, 1, "MORE"); button(t, 2, "HUNT");
}
void hunt(TFT_eSPI& t) {
    header(t, "HUNTER"); const auto* r = goblinBleInventory().find(selected);
    if (!r) { line(t, 55, "Observation expired / evicted"); button(t, 0, "BACK"); return; }
    line(t, 39, bleClassName(r->advertisement.classification), TFT_CYAN); line(t, 56, addr(r->key));
    line(t, 76, "Current: " + String(r->rssi) + " dBm / best: " + String(r->strongest_rssi));
    t.setTextSize(2); t.setTextDatum(MC_DATUM); t.setTextColor(TFT_CYAN, TFT_BLACK);
    t.drawString(goblinRadioNow() - r->last_seen > 30000 ? "STALE SIGNAL" : bleSignalCategory(r->rssi), 160, 113);
    line(t, 142, "Last seen: " + age(r->last_seen)); line(t, 162, "Seen count: " + num(r->seen_count));
    line(t, 185, "Approximate signal strength, not distance.", TFT_YELLOW);
    button(t, 0, "BACK"); button(t, 1, "DETAILS"); button(t, 2, "BLUE");
}
void privacy(TFT_eSPI& t) {
    header(t, "PRIVACY"); const auto* a = findAlert();
    if (!a) { line(t, 65, "No active privacy alert for this observation."); button(t, 0, "BLUE"); button(t, 1, "NEXT"); return; }
    selected_alert = *a; selected = a->ble.key;
    line(t, 39, a->type == GuardAlertType::BlePersistentTrackerCandidate ? "PERSISTENT TRACKER-CAPABLE" : "TRACKER-CAPABLE DEVICE", TFT_YELLOW);
    line(t, 56, a->type == GuardAlertType::BlePersistentTrackerCandidate ? "DEVICE NEARBY" : "OBSERVED");
    line(t, 77, String(bleClassName(a->ble.classification)) + " / " + bleConfidenceName(a->ble.confidence));
    line(t, 97, addr(a->ble.key));
    line(t, 116, "RSSI: " + String(a->ble.rssi) + " / seen: " + num(a->ble.seen_count));
    line(t, 135, "Observed span: " + num((a->ble.last_seen - a->ble.first_seen) / 60000) + " min");
    line(t, 154, "Last seen: " + age(a->ble.last_seen));
    line(t, 180, "Nearby does not prove unwanted tracking.", TFT_YELLOW);
    button(t, 0, "HUNT"); button(t, 1, "DETAILS"); button(t, 2, "DISMISS");
}
}
bool goblinBlueActive() { return active; }
void goblinBlueRefresh(TFT_eSPI& t) {
    refreshed = goblinRadioNow();
    switch (page) { case Page::Overview: overview(t); break; case Page::Devices: devices(t); break; case Page::Details: details(t); break; case Page::Hunt: hunt(t); break; case Page::Privacy: privacy(t); break; }
}
void goblinBlueShow(TFT_eSPI& t) { active = true; page = Page::Overview; goblinBlueRefresh(t); }
void goblinBlueShowAlert(TFT_eSPI& t, const GuardAlert& alert) { active = true; page = Page::Privacy; selected_alert = alert; selected = alert.ble.key; goblinBlueRefresh(t); }
GoblinUiAction goblinBlueTouch(TFT_eSPI& t, int16_t x, int16_t y) {
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return GoblinUiAction::None;
    if (y < 32) { if (page == Page::Overview) active = false; else { page = Page::Overview; goblinBlueRefresh(t); } return GoblinUiAction::None; }
    if (page == Page::Devices && y >= 37 && y < 190) {
        const size_t slot = (y - 37) / 51;
        if (slot < visible_count) { selected = visible[slot]; detail_page = 0; page = Page::Details; goblinBlueRefresh(t); }
        return GoblinUiAction::None;
    }
    if (y < 205 || y >= 237) return GoblinUiAction::None;
    const int slot = x >= 4 && x <= 103 ? 0 : x >= 110 && x <= 209 ? 1 : x >= 216 && x <= 315 ? 2 : -1;
    if (slot < 0) return GoblinUiAction::None;
    switch (page) {
    case Page::Overview:
        if (slot == 0) { page = Page::Devices; list_page = 0; }
        else if (slot == 1) { nextAlert(); page = Page::Privacy; }
        else return goblinRadio().enabled() ? GoblinUiAction::StopMonitor : GoblinUiAction::StartMonitor;
        break;
    case Page::Devices:
        if (slot == 0) page = Page::Overview;
        else if (slot == 1 && list_page) --list_page;
        else if (slot == 2 && (list_page + 1) * 3 < goblinBleInventory().count()) ++list_page;
        break;
    case Page::Details:
        if (slot == 0) page = Page::Devices; else if (slot == 1) detail_page = (detail_page + 1) % 3; else page = Page::Hunt;
        break;
    case Page::Hunt: page = slot == 2 ? Page::Overview : Page::Details; break;
    case Page::Privacy:
        if (!findAlert()) { if (slot == 1) nextAlert(); else page = Page::Overview; }
        else if (slot == 0) page = Page::Hunt;
        else if (slot == 1) { detail_page = 0; page = Page::Details; }
        else {
            for (size_t i = 0; i < goblinGuardAlerts().count(); ++i) {
                const auto* a = goblinGuardAlerts().at(i);
                if (guardBleAlert(a->type) && a->type == selected_alert.type && bleSameKey(a->ble.key, selected_alert.ble.key)) {
                    goblinLogDismiss(*a);
                    goblinGuardAlerts().dismiss(i);
                }
            }
            nextAlert();
        } break;
    }
    goblinBlueRefresh(t); return GoblinUiAction::None;
}
void goblinBlueTick(TFT_eSPI& t) { if (goblinRadioNow() - refreshed >= 500) goblinBlueRefresh(t); }
