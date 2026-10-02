# Phase 4 — BLE Privacy Watch

Phase 2 trusted AP storage and Phase 3 AIR monitoring are preserved. The display,
FT6336 touch driver, board definition, partitions and pioarduino platform are
unchanged. NimBLE-Arduino is pinned to **2.5.1**. Only its observer role is enabled;
central, peripheral and broadcaster roles are disabled. Extended scanning is
enabled to receive longer supported advertisements. There is no BLE connection,
pairing, GATT, advertising or active scan request path.

## Operation

Tap the hardware screen to scan APs, tap **AIR >**, and press **START**. START
enables the alternating radio cycle, not just Wi-Fi. Tap **BLUE >** in AIR's
header to open Privacy Watch. The BLUE overview provides DEVICES, PRIVACY and
START/STOP. Its header returns to AIR. AIR's channel view still links to GUARD.

The device list is paged, three observations per page. A row opens three detail
pages; MORE cycles metadata, timestamps/persistence, and advertised services.
HUNT shows current/best RSSI, count and age. The signal becomes STALE after 30
seconds without an observation. Privacy alerts provide HUNT, DETAILS and DISMISS.
Tap a BLE subpage header to return to BLUE. Dismissing an observed-device alert
does not suppress a later persistence escalation. Historical/stale BLE alerts
expire after 120 seconds without qualifying advertisements.

Nearby overview counts use a 60-second last-seen interval. The runtime list also
includes older retained records, always with last-seen age. Unknown includes
generic BLE without a recognized protocol signature. Missing names, company IDs
and services are explicitly unavailable. Company lookup contains only Apple;
all other IDs are displayed numerically. Services are displayed most-significant
byte first; up to four distinct 16-, 32- or 128-bit UUIDs are retained.

## Radio ownership and recovery

`RadioCoordinator` owns Idle, WifiMonitor, BlePrivacyScan and WifiApScan changes.
The pure coordinator uses a test backend; `goblin_ble.cpp` adapts it to ESP32.
Constants in `radio_coordinator.h` are provisional hardware-tuning values:

| Setting | Value |
|---|---:|
| Wi-Fi monitor window | 5,000 ms |
| BLE privacy window | 2,500 ms |
| Failed transition retry | 1,000 ms |
| BLE interval / receive window | 100 / 80 ms |

Promiscuous reception stops before BLE starts. BLE stops before Wi-Fi resumes.
An AP scan stops either listener and restores its previous mode even after empty
or failed discovery; an idle scan returns to idle. Failed BLE restoration falls
back to Wi-Fi and reports that the previous mode could not be restored exactly.
The existing AP discovery API can transmit Wi-Fi probes; this phase adds only
passive BLE reception, not an entirely RF-silent replacement for AP discovery.

Initial NimBLE host synchronization runs in one temporary 6,144-byte-stack task.
The first BLE window may be deferred while it initializes; Wi-Fi resumes during
that wait, and the next scheduled window retries. Even a stalled host init does
not block touch or permanently pause Wi-Fi. A returned init failure disables BLE
for the boot and logs subsequent attempts; restart is needed to retry that stack
initialization. NVS is checked before calling NimBLE to avoid its NVS erase
fallback. No BLE data is persisted or written to the trusted AP store.

Stop failure triggers stack deinitialization, preserving the scan object until
shutdown succeeds. If both stop and reset fail, Wi-Fi must wait to avoid two
active listeners; cleanup retries every second and restores Wi-Fi when safe.
This is reported as a transition failure, not claimed as successful recovery.
The scan-end callback never restarts reception. Upstream init/shutdown execution
and actual RF recovery still require the hardware soak test below.

## Data, bounds and interpretation

| Storage | Capacity / policy |
|---|---|
| BLE SPSC event queue | 64 observations; drop incoming on overflow |
| Advertisement copy | 64 bytes/event; truncate and count larger payloads |
| BLE device inventory | 64 address-plus-type records |
| Privacy candidates | 16 records |
| BLE alert share | At most 8 of the existing 32 GuardAlert entries |
| Name / services | 32 printable bytes / 4 unique UUIDs per record |

Callback work is timestamp/address/type/RSSI/payload copying and atomic queue
bookkeeping. Parsing, classification, alerts, rendering and diagnostics happen
on the main task. NimBLE uses callback-only results (`setMaxResults(0)`), passive
scans and duplicate callbacks. Its internal per-report allocations remain library
managed; the project does not keep an unbounded results vector. The queue is
single producer (NimBLE host callback), single consumer (main loop).

Inventory and candidates evict the oldest non-persistent entry first. If every
entry is protected, the incoming observation/candidate is dropped. BLE alerts
prefer replacing dismissed/non-persistent BLE entries; active persistent BLE
alerts are retained when their share is full. Existing higher-priority Wi-Fi
alerts retain their normal priority policy. Drops and evictions are reported in
five-second serial diagnostics, not with a log per advertisement.

Address and address type form an observation key, never a permanent physical
identity. Random addresses are marked random; public addresses are labeled public
without a permanence claim. Similar RSSI, company or payload does not link two
addresses. Persistence requires **at least three separate BLE windows spanning
600,000 ms**, with no gap reaching **120,000 ms**. Reboot, expiry or candidate
eviction resets that observation's persistence evidence. Rotation can prevent
persistence detection. Pauses and dropped advertisements can cause false negatives.

Registry matchers use documented FEAA service data. Find Hub 0x40 accepts a
20- or 32-byte identifier with optional flags; 0x41 requires flags. Eddystone UID,
URL, TLM (plain/encrypted format) and EID are beacon classifications. Unknown FEAA
types stay generic. Apple manufacturer ID alone never means AirTag/Find My.
iBeacon is reserved in the enum but has no Phase 4 signature. No owner, location
or encrypted data is decoded. Identifiers are not added to inventory or alerts;
bounded queue payload slots are overwritten as new events arrive.
See the source links and licenses in [ATTRIBUTIONS](../ATTRIBUTIONS.md).

HIGH means that bytes fit a documented protocol format, not authenticated
participation or malicious intent. Neither proximity nor persistence proves
unwanted tracking. RSSI categories are approximate signal strength: below -85
very weak, -85 to -76 weak, -75 to -66 medium, -65 to -51 strong, and -50 or higher
very strong. No distance estimate is made. Walls, antenna orientation, body
blocking, transmit power and multipath affect readings.

## Validation and limitations

Run all host suites with `.pio/venv/Scripts/python.exe scripts/run_host_tests.py`.
The BLE suite covers bounded/malformed AD structures, duplicate/order handling,
names/company/services, frame type/length matrices, confidence, identity, record
updates/capacity, protected eviction, persistence thresholds/expiry, alert context
and dismissal, RSSI boundaries, concurrent queue operation, radio transitions,
init deferral, start/stop/reset failures and AP scan restoration. Synthetic bytes
and deterministic timestamps need no BLE hardware or transmitted test packets.

Build using `pio run -e hosyond_es3c28p`. Build statistics measure flash and static
internal RAM, not peak runtime heap/PSRAM. Phase 4 adds no explicit PSRAM allocation;
framework/library allocation and minimum free memory must be measured on-device.
Extended advertisement coverage, touch layout, actual radio timing, long-running
memory behavior and real protocol detection remain hardware acceptance items.
No desktop host test proves reception sensitivity or absence of missed frames.

## Manual ES3C28P acceptance — at least 30 minutes

1. Flash the merged image to **ESP32-S3 at offset 0x0000**, then capture serial
   output at 115200 baud. Boot and verify display/touch plus the trusted AP baseline.
2. Run AP discovery, enter AIR, and START. Open BLUE using AIR's header. Allow
   initial BLE host setup; confirm repeating approximately 5-second Wi-Fi and
   2.5-second BLE windows in serial and the BLUE active/paused/countdown display.
3. Confirm Wi-Fi counters continue updating after every BLE window. Check both
   START/STOP controls and idle behavior; no scan should restart from a callback.
4. Use ordinary personally owned BLE devices already nearby. Confirm observations,
   changing RSSI, address types, names/manufacturer when advertised, device paging,
   all detail pages and Hunter. Move/orient the receiver to observe RSSI changes;
   do not interpret its category as a measured distance.
5. If a personally owned compatible device happens to be present, check factual
   protocol wording and alert dismissal. A device purchase, tracker deployment or
   simulated malicious tracking is unnecessary; threshold behavior is host tested.
6. During both a Wi-Fi and a BLE window, return to AIR and run AP SCAN. Confirm
   listeners pause and the previous mode resumes. Repeat from STOP/idle; it should
   stay idle after the scan. Recheck saved trusted APs.
7. Run for at least **30 minutes**, periodically paging and touching controls.
   Record Wi-Fi queue drops, BLE queue drops, inventory/candidate evictions/drops,
   transition failures, minimum free heap and minimum free PSRAM from diagnostics.
8. Confirm no spontaneous reboot, stalled scan cycle or loss of touch response.
   Stop/start again after the soak. Save the serial log with duration and observed
   device count. If memory trends down or failures recur, retain the log and tune
   the named window constants only after diagnosing the cause.

Hardware results are intentionally unclaimed until this procedure is performed.
