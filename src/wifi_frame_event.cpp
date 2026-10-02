#include "wifi_frame_event.h"
#include <cstring>

bool parseWifiManagementFrame(const uint8_t* frame, size_t length, uint64_t timestamp_us,
                             int8_t rssi, uint8_t channel, WifiFrameEvent& out) {
    out = WifiFrameEvent{};
    if (!frame || length < 24 || channel < 1 || channel > 14) return false;
    const uint16_t fc = frame[0] | (static_cast<uint16_t>(frame[1]) << 8);
    // Version 0, management type, no ToDS/FromDS; no reassembly in this phase.
    if ((fc & 0x030F) || (fc & 0x0400) || (frame[22] & 0x0F)) return false;
    const uint8_t subtype = (fc >> 4) & 0x0F;
    size_t fixed_body = 0;
    switch (subtype) {
        case 0: fixed_body = 4; break;  // association request
        case 1: case 3: fixed_body = 6; break;
        case 2: fixed_body = 10; break; // reassociation request
        case 4: break;                 // probe request
        case 5: case 8: fixed_body = 12; break;
        case 10: case 12: fixed_body = 2; break;
        case 11: fixed_body = 6; break; // authentication
        case 13: case 14: fixed_body = 1; break; // action / action-no-ack
        default: return false;         // reserved/unsupported format
    }
    size_t header = 24;
    if (fc & 0x8000) {
        // HT Control is supported only for Action / Action No Ack management frames.
        if (subtype != 13 && subtype != 14) return false;
        header += 4;
    }
    if (length < header || length - header < fixed_body) return false;
    out.timestamp_us = timestamp_us;
    out.subtype = subtype;
    out.rssi = rssi;
    out.channel = channel;
    out.protected_frame = (fc & 0x4000) != 0;
    std::memcpy(out.destination, frame + 4, 6);
    std::memcpy(out.source, frame + 10, 6);
    std::memcpy(out.bssid, frame + 16, 6);
    // Protected bodies may be encrypted. Never interpret ciphertext as a reason.
    if ((subtype == 10 || subtype == 12) && !out.protected_frame) {
        out.reason_code = frame[header] | (static_cast<uint16_t>(frame[header + 1]) << 8);
        out.has_reason_code = true;
    }
    return true;
}
