# Feature Provenance

Every substantial feature must have an entry here before public release.

| Feature | Status | Implementation | Reference studied | Copied source | License impact |
|---|---|---|---|---|---|
| ES3C28P display bring-up | Alpha | ESP Goblin original | LCDWiki, Bruce board port | No | TFT_eSPI MIT |
| FT6336G touch | Alpha | ESP Goblin original | public FT6336 register behavior, Bruce board validation | No | None |
| Goblin Guard AP inventory | Alpha | Original BSSID-keyed in-RAM records merged from Arduino WiFi scans; new-AP serial events and inventory UI counts | Existing project Arduino WiFi scan integration | No | Framework; no new dependency |
| Goblin Guard persistent trusted baseline (Phase 2) | Alpha | Original ESP Goblin trusted model, versioned/checksummed NVS blob, trust/forget UI, baseline comparison and SSID collision alerts | Locally installed ESP32 Arduino Preferences/NVS APIs and Wi-Fi auth enum | No; no Wireless Wizard, HaleHound, Piglet or other project code imported | Existing Arduino/ESP32 framework; no new firmware dependency |
| Passive management-frame air monitor and disconnect-burst detector (Phase 3) | Alpha; hardware soak pending | Original ESP Goblin bounded metadata parser, fixed SPSC queue, country-aware hopping, counters, rolling detector and UI | Official Espressif promiscuous receive APIs/installed ESP-IDF headers; IEEE 802.11 management-header structure | No implementation code imported from Wireless Wizard, HaleHound, Piglet, Bruce, Marauder, GhostESP or other firmware | Existing Arduino/ESP32 framework and standard C++; no new firmware dependency |
| BLE Privacy Watch (Phase 4) | Alpha; hardware soak pending | Original bounded AD parser, address/type inventory, provenance registry, conservative persistence, BLE alert context and touch views | Google Find Hub public specification, Google Eddystone specification, Bluetooth SIG assigned numbers, NimBLE 2.5.1 API | No firmware implementation copied | NimBLE-Arduino Apache-2.0; specification/data notices in ATTRIBUTIONS.md |
| Flock signature detection | Planned | TBD | flock-you | No currently | MIT if reused |
| Remote ID | Planned | Spec-based | ASTM/OpenDroneID | No currently | Apache/spec |
| BLE spam detection | Planned | TBD | Wall of Flippers | No currently | MIT if reused |
| RadioCoordinator (Phase 4) | Alpha; hardware timing provisional | Original hardware-independent exclusive radio transitions, asynchronous BLE initialization and explicit recovery | Installed NimBLE 2.5.1 and ESP32 APIs | No | Existing framework plus NimBLE dependency |
| SD event history (Phase 5) | Alpha; SD mount not yet confirmed on this ES3C28P | Original bounded JSONL session log, NVS session counter, history, export, retention and integrity checksum | Arduino-ESP32 SD_MMC, Preferences/NVS, and FIPS 180-4 SHA-256 | No logging or storage implementation copied | Existing Arduino-ESP32 SD_MMC and NVS; no new firmware dependency |
| Wardriving mesh | Planned | Original protocol unless compatible open source selected | Piglet concept only | No | Avoid NC code |

## Goblin Guard behavior

Goblin Guard retains BSSID, SSID, channel, RSSI, numeric framework auth mode,
first_seen/last_seen (64-bit monotonic milliseconds since boot), seen_count
(once per scan), and Known/Unknown state. Newly observed BSSIDs default to
Unknown unless an enabled persisted baseline matches their BSSID. Matching SSIDs do not combine APs; repeat BSSIDs update the existing
record and preserve first_seen and state. Empty or failed scans retain records;
the new count resets each scan. Records remain until reboot, including APs no
longer visible. The board-independent inventory uses checked heap allocation;
allocation failures are logged and the screen reports an incomplete inventory.
Phase 2 separates transient inventory, `TrustedApStore`, `GuardAnalyzer`, the NVS
adapter, and TFT rendering/touch actions. Only explicitly trusted configuration
is persisted; boot-relative observation timestamps are never serialized. Trust
and forget operations update runtime classification only after successful saves.
Enabled BSSID matches restore Known state after reboot and scanning.

SSID/security/channel changes are compared with the saved baseline, without
automatically accepting changed values. Unknown BSSIDs with an exact, nonempty
trusted SSID generate SSID_COLLISION; this is not proof of an attack. Channel
changes are informational. Alerts are volatile and bounded. Scan-derived alerts reset each scan; passive Wi-Fi and BLE alerts survive scan pauses.
See [Phase 2 operation and validation](GOBLIN_GUARD.md) for capacities, recovery
behavior, tests, and hardware checks.

Phase 3 adds management-only promiscuous reception and factual disconnect-burst
observations. It copies metadata only, with no retained packet payload, PCAP,
credential collection or transmission/injection feature. Phase 3 does not write
the SD card; the promiscuous callback still returns after one queue copy. See
[AIR MONITOR](AIR_MONITOR.md) for callback constraints, test coverage, limits and
the physical acceptance procedure. Phase 2 NVS baseline behavior is preserved.

## Import rule

- MIT/BSD/Apache/public standards: may be incorporated with notices.
- GPL-compatible code: may be incorporated deliberately with provenance.
- Proprietary / non-commercial / unclear licensing: do not copy source.

## Phase 4 BLE provenance and scope

`ble_signatures.cpp` implements original matchers with identifier, classification,
confidence, source URL, license description and a match function in a single
registry. Protocol-format confidence does not establish authenticity or intent.
Google Find Hub matching follows the [public advertised-frame specification](https://developers.google.com/nearby/fast-pair/specifications/extensions/fmdn).
Only FEAA service data with the specified 0x40/0x41 types and valid lengths
matches; there is no identifier decryption, owner derivation or cross-address linking.
[Eddystone](https://github.com/google/eddystone) UID, URL, TLM and EID rules are
separate beacon classifications. Apple company ID alone remains generic BLE.
iBeacon and other enum values reserve future extension; no undocumented Apple
signature is included.

The parser, inventory, analyzer and coordinator have no ESP32 or display dependency.
NimBLE callbacks only copy bounded observations into a fixed queue. The ESP32
adapter owns stack operations; the coordinator owns mode changes. Phase 2 trust
storage and Phase 3 frame capture remain intact. No BLE connections, advertising,
GATT, SD history or active BLE scan requests are added. The pre-existing AP scan
is still the normal Wi-Fi discovery API and may transmit probe requests.

See [BLE Privacy Watch operation and acceptance](BLE_PRIVACY_WATCH.md) for
capacities, thresholds, recovery, tests, limitations, and hardware soak procedure.

## Phase 5 SD event history

Phase 5 adds durable observation history. It does not add a radio mode, PCAP,
packet injection, BLE connections, GPS, a web dashboard, OTA, or any upload.

`StorageManager` is the only SD_MMC owner. It mounts the ES3C28P bus in 1-bit
mode using `GoblinBoard::SD_CLK`, `SD_CMD`, and `SD_D0`, then creates `/goblin/`.
The D1/D2/D3 board constants stay unused until 4-bit mode is proven on hardware.
Guard, Air, BLE, and the display submit `EventRecord` values. They do not open
files. Wi-Fi and BLE callbacks still only enqueue their existing observation
queues.

The canonical log is `/goblin/sessions/session_NNNNNN.jsonl`. Session numbers
come from Preferences/NVS key `next` in namespace `goblin_log`, and from the
highest session file if that counter cannot be read. Wall-clock fields stay
absent unless `gettimeofday` returns a plausible UTC instant. `millis()` is
stored as `mono_ms` and is never treated as Unix time.

JSONL serialization, deduplication, retention selection, CSV escaping, path
checks, and SHA-256 are original and host-tested. SHA-256 follows FIPS 180-4
and is used only as an integrity checksum for a closed session file. It is not
a chain of custody. No Wireless Wizard, Bruce, Marauder, GhostESP, HaleHound,
or Piglet logging code was copied.

See [EVENT_HISTORY.md](EVENT_HISTORY.md).
