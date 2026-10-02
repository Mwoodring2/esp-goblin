#include "ble_privacy.h"
namespace {
// Match service-data bytes AFTER the little-endian FEAA UUID. Original rules,
// derived from the linked public specifications, never owner/identifier decoding.
bool findHub(const uint8_t* p, size_t n) {
    return n && ((p[0] == 0x40 && (n == 21 || n == 22 || n == 33 || n == 34)) ||
                 (p[0] == 0x41 && (n == 22 || n == 34)));
}
bool uid(const uint8_t* p, size_t n) { return n == 20 && p[0] == 0x00 && p[18] == 0 && p[19] == 0; }
bool url(const uint8_t* p, size_t n) {
    if (n < 4 || n > 20 || p[0] != 0x10 || p[2] > 3) return false;
    for (size_t i = 3; i < n; ++i) if (p[i] > 13 && (p[i] < 0x21 || p[i] > 0x7e)) return false;
    return true;
}
bool tlm(const uint8_t* p, size_t n) { return n >= 2 && p[0] == 0x20 && ((p[1] == 0 && n == 14) || (p[1] == 1 && n == 18)); }
bool eid(const uint8_t* p, size_t n) { return n == 10 && p[0] == 0x30; }
const BleSignature signatures[] = {
    {"find-hub", BleDeviceClass::GoogleFindHub, DetectionConfidence::High, "https://developers.google.com/nearby/fast-pair/specifications/extensions/fmdn", "Specification: CC BY 4.0; original matcher", 0x16, 0xfeaa, findHub},
    {"eddystone-uid", BleDeviceClass::Eddystone, DetectionConfidence::High, "https://github.com/google/eddystone/tree/master/eddystone-uid", "Apache-2.0 specification; original matcher", 0x16, 0xfeaa, uid},
    {"eddystone-url", BleDeviceClass::Eddystone, DetectionConfidence::High, "https://github.com/google/eddystone/tree/master/eddystone-url", "Apache-2.0 specification; original matcher", 0x16, 0xfeaa, url},
    {"eddystone-tlm", BleDeviceClass::Eddystone, DetectionConfidence::High, "https://github.com/google/eddystone/tree/master/eddystone-tlm", "Apache-2.0 specification; original matcher", 0x16, 0xfeaa, tlm},
    {"eddystone-eid", BleDeviceClass::Eddystone, DetectionConfidence::High, "https://github.com/google/eddystone/tree/master/eddystone-eid", "Apache-2.0 specification; original matcher", 0x16, 0xfeaa, eid}
};
}
const BleSignature* bleSignatures(size_t& count) { count = sizeof(signatures) / sizeof(signatures[0]); return signatures; }
void classifyBleAdStructure(BleParsedAdvertisement& a, uint8_t type, const uint8_t* p, size_t n) {
    if (!p) return;
    for (const auto& signature : signatures) {
        if (type != signature.ad_type) continue;
        const size_t prefix = signature.service_uuid ? 2 : 0;
        if (n < prefix) continue;
        if (prefix && (p[0] | (uint16_t(p[1]) << 8)) != signature.service_uuid) continue;
        if (!signature.matches(p + prefix, n - prefix)) continue;
        // A later beacon field must not hide an independently valid Find Hub field.
        if (a.classification != BleDeviceClass::GoogleFindHub) {
            a.classification = signature.classification; a.confidence = signature.confidence; a.signature = signature.identifier;
        }
        return;
    }
}
