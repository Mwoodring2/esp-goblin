# Goblin Guard Phase 2

## Operation

Tap the hardware screen to scan. Guard shows Inventory, Known, Unknown, New this
scan, Alerts, and the existing AIR SNIFFER scan metrics. RESCAN performs another
normal Wi-Fi scan. VIEW APs opens a three-entry page with PREV/NEXT controls.
Each entry shows its SSID (or friendly name), BSSID, RSSI, and Known/Unknown state.
Empty SSIDs display as Hidden. BSSIDs distinguish APs sharing the same SSID.

Tap an entry for SSID, BSSID, RSSI, channel, security, seen count, state, and whether
it was present in the latest scan. TRUST THIS AP captures that observed SSID,
auth mode, and channel. FORGET AP deletes its saved baseline. These actions show
a success or failure message. A held finger produces only one action, so a
trust action cannot immediately become a forget action.

An important security change or SSID collision opens an alert automatically.
ALERTS opens all remaining events, including new APs and informational channel
changes. DETAILS opens the observed AP; DISMISS acknowledges one event;
NEXT/BACK advances through events and then returns to Guard. Dismissing does not
modify trust. Persistent differences will be reported again on the next scan.
Changing trust reevaluates the latest observations without adding new-AP events.

## Baseline and failure behavior

- Up to 32 records, identified only by six-byte BSSID.
- Expected SSID (32 bytes), numeric auth mode, expected channel, optional friendly
  name (32 bytes), and enabled flag. This reference board supports channels 1–14.
- Namespace `goblin_guard`, key `baseline`, one NVS blob written with Preferences.
- Schema 1: 12-byte header (`GGRD`, little-endian version/count, CRC32), followed
  by 76-byte records with explicit field encoding. No raw C++ struct serialization.
- CRC covers the header and records excluding the CRC field itself. Loads check
  length, magic, version, capacity, string termination, BSSID validity/uniqueness,
  channel, and boolean values before publishing any records.
- Missing storage means an empty baseline. Read errors, corrupt data, or an
  unsupported version leave all APs Unknown and disable trust edits for that boot;
  the UI and serial report the error and the rest of firmware continues running.
  No automatic erasure or migration is attempted. Recovery requires restoring
  compatible baseline data or explicitly clearing that namespace with a separate
  maintenance tool and rebooting; this phase does not add a reset UI.
- Failed writes are logged, return false, and roll back the in-RAM baseline edit;
  the UI does not claim that the operation succeeded.
- Duplicate trust is idempotent and never silently replaces an enabled baseline.
  To intentionally accept changed observations, forget and trust the AP again.
- Disabled records remain stored but confer no trust and do not serve as collision
  candidates. `configureTrustedAccessPoint(bssid, name, enabled)` manages friendly
  names/enabled state; this phase provides trust/forget touch controls only.
- No passwords, scan inventory, alerts, or boot-relative timestamps are saved.

## Alert rules and limits

`GuardAnalyzer` has no TFT, Arduino, NVS, or board-pin dependency. It compares
only APs actually observed in the latest scan, so missing APs do not generate
stale change alerts. Stored baselines are never updated by scanning.

An enabled trusted BSSID can generate SSID_CHANGED, SECURITY_CHANGED, and
CHANNEL_CHANGED independently. All auth-mode differences are reported, including
downgrades to OPEN. Events contain old/new values and the observed radio context.
CHANNEL_CHANGED is informational because routers legitimately change channels.
SSID_COLLISION requires an unknown BSSID and exact case-sensitive, nonempty SSID
matching an enabled baseline. Multiple trusted BSSIDs may legitimately share one
SSID. Empty hidden SSIDs are deliberately excluded from name collision checks.
No event claims that an attack is proven.

At most 32 events are retained per scan. Security changes are prioritized over
collisions, which are prioritized over SSID/channel changes and new-AP notices.
Overflow is logged and indicated in the summary; not every event can be retained
in a dense environment. Alerts reset on each scan, including empty/failed scans;
scan failures remain visibly marked, and inventory/baselines are retained.
No background scan scheduler is added. Observations change only after a manual scan.

The baseline, serialization buffer, and alert buffer use fixed ordinary RAM.
They do not require PSRAM. Existing inventory records allocate from the heap as
new BSSIDs arrive, and the existing checked-allocation behavior remains. Build
RAM figures exclude dynamic inventory, framework allocations, and task stacks.

## Validation

Host tests require a C++11 compiler and no physical ESP32:

```text
c++ -std=c++11 -Wall -Wextra -Iinclude src/access_point_inventory.cpp src/trusted_ap_store.cpp src/guard_analyzer.cpp tests/guard_baseline_test.cpp -o guard_test
./guard_test
c++ -std=c++11 -Iinclude src/access_point_inventory.cpp tests/access_point_inventory_test.cpp -o inventory_test
./inventory_test
```

The Phase 2 tests cover trust/untrust, BSSID lookup, restore after reboot, enabled
state/friendly names, exact/nonmatching SSIDs, changed SSID/auth/channel, duplicate
operations, capacity, invalid inputs, failed reads/writes, rollback, corrupted or
incompatible blobs, event context/deduplication/dismissal, and alert overflow.

Firmware build:

```text
pio run -e hosyond_es3c28p
```

Output: `dist/ESP-Goblin-ES3C28P-v0.1.0-alpha.0.bin`, a merged ESP32-S3 image flashed
at `0x0000`. Bootloader is at `0x0000`, partitions at `0x8000`, boot_app0 at
`0xe000`, and application at `0x10000`. Preserve NVS when reflashing if you want
to retain trust: a full-chip erase removes the baseline.

Physical validation remains necessary: trust an AP, reboot without erasing NVS,
rescan and verify Known; forget and reboot to verify Unknown; navigate pages and
test press/release behavior; change your own test AP's SSID, auth mode and channel
and verify old/new alerts. Confirm a second test AP with the same SSID produces
SSID_COLLISION, and a differently named AP does not. Host tests and compilation
do not substitute for physical display/touch or NVS power-cycle testing.
