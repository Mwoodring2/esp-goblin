#include "ui.h"
#include "app_version.h"

namespace {

void drawHeader(TFT_eSPI& tft) {
    tft.fillRect(0, 0, tft.width(), 44, TFT_DARKGREEN);
    tft.setTextColor(TFT_GREEN, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("ESP GOBLIN", tft.width() / 2, 22);
}

void drawMetric(
    TFT_eSPI& tft,
    int x,
    int y,
    int w,
    const char* label,
    const String& value
) {
    tft.drawRoundRect(x, y, w, 66, 8, TFT_DARKGREY);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString(label, x + 10, y + 8);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(value, x + 10, y + 30);
}

}  // namespace

void drawBootScreen(TFT_eSPI& tft, const BoardProfile& board) {
    tft.fillScreen(TFT_BLACK);
    drawHeader(tft);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("WAKING THE GOBLIN...", tft.width() / 2, 105);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString(board.display_name, tft.width() / 2, 150);
    tft.drawString(ESP_GOBLIN_VERSION, tft.width() / 2, 170);
}

void drawDashboard(
    TFT_eSPI& tft,
    const BoardProfile& board,
    const WifiSnapshot& snapshot
) {
    tft.fillScreen(TFT_BLACK);
    drawHeader(tft);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("AIR SNIFFER / INITIAL RADIO CHECK", 12, 56);

    const int gap = 8;
    const int margin = 10;
    const int card_w = (tft.width() - (margin * 2) - gap) / 2;

    drawMetric(tft, margin, 82, card_w, "ACCESS POINTS", String(snapshot.access_point_count));
    drawMetric(tft, margin + card_w + gap, 82, card_w, "OPEN NETWORKS", String(snapshot.open_network_count));

    drawMetric(tft, margin, 156, card_w, "STRONGEST RSSI", String(snapshot.strongest_rssi) + " dBm");
    drawMetric(tft, margin + card_w + gap, 156, card_w, "BUSIEST CHANNEL", String(snapshot.busiest_channel));

    tft.drawRoundRect(margin, 238, tft.width() - (margin * 2), 62, 8, TFT_DARKGREY);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("GOBLIN GUARD", margin + 10, 250);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Baseline learning + alerts come next.", margin + 10, 270);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(board.id, margin + 10, tft.height() - 18);
}
