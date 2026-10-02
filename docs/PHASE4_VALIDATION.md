# Phase 4 validation — 2026-09-30

Phase 4 implementation is complete locally. Phase 2/3 working-tree changes were
preserved. No commit, push, physical flash or hardware acceptance is claimed.
No later-phase work was started.

## Build and tests

- `pio run -e hosyond_es3c28p`: SUCCESS, 42.21 seconds in the final run.
- `.pio/venv/Scripts/python.exe scripts/run_host_tests.py`: all four suites PASS.
- BLE privacy/coordinator: **6,810 checks, zero failures**, including 6,500 bounded
  random parser cases and a 20,000-event concurrent queue exercise.
- Trusted baseline/analyzer: **86 checks, zero failures**.
- Phase 3 AIR: **1,378 checks, zero failures**.
- AP inventory suite: PASS (identity, rescan merge, metadata, counts, state, invalid data).
- `git diff --check`: passed; Git emitted line-ending conversion notices only.
- No project `assert()` statements added. Runtime failures use checks/logging.
- CI was not rerun for these unpushed local changes.

## Firmware and memory

| Item | Result |
|---|---:|
| Flash | 1,264,035 / 6,553,600 bytes (19.3%) |
| Static internal RAM | 101,284 / 327,680 bytes (30.9%) |
| Additional explicit PSRAM allocation | None |
| Runtime free/minimum heap and PSRAM | Instrumented; physical measurements pending |
| BLE queue | 64 events, payload at most 64 bytes each |
| BLE inventory | 64 records |
| Privacy candidates | 16 records |
| BLE alerts | At most 8 within the existing 32-entry envelope |
| Radio cycle | Wi-Fi 5,000 ms / passive BLE 2,500 ms |
| BLE interval/window | 100 ms / 80 ms |

Runtime NimBLE buffers, its tasks and the temporary 6,144-byte initialization-task
stack are additional to the static-RAM figure. No measured peak RAM/PSRAM claim
can be made without the on-device soak. Diagnostics report current/free minimum
heap and PSRAM, radio uptime, observations, queue drops, eviction/candidate drops
and transition failures every five seconds.

The exact new dependency is `h2zero/NimBLE-Arduino@2.5.1`. It builds on the existing
pioarduino 55.03.39 platform / Arduino-ESP32 3.3.9, without a downgrade. Observer
role and extended scanning are enabled; central, peripheral and broadcaster roles
are disabled. Board pins, display driver, touch driver, partitions and flash/PSRAM
configuration are unchanged.

## Merged image verification

- Output: `dist/ESP-Goblin-ES3C28P-v0.1.0-alpha.0.bin`
- Target: **ESP32-S3**, flash the merged image at **0x0000**.
- Size: 1329968 bytes.
- SHA-256: `284a59b31718b6a20279a1c085147946b24c698f5ec65c1957474e107f9c155c`.
- Byte-for-byte checks passed against the build's bootloader at `0x0000`,
  partitions at `0x8000`, boot_app0 at `0xe000`, application at `0x10000`.
- Image magic and chip ID 9 confirm ESP32-S3.

## Warnings and remaining validation

Existing TFT_eSPI `TOUCH_CS` warning refers to its unused resistive-touch support;
the separate FT6336 capacitive-touch driver is unchanged. The merger emits
esptool deprecated-option spelling warnings. PlatformIO emits a Windows terminal
codepage notice, but its flash/RAM size check and image generation both succeed.

No real-hardware reception, RF scheduling/timing, UI layout or 30-minute stability
result is claimed. Extended-advertisement reception must be checked physically.
BLE init may defer the first window while Wi-Fi continues. If BLE stop and reset
both fail, exclusive ownership requires retrying cleanup before Wi-Fi can resume.
Stalled initialization cannot block the main/UI task; a returned init failure
leaves BLE disabled for the boot. No AirTag/Find My or iBeacon signature is present.
Protocol confidence does not prove authenticity or intent; rotating addresses may
prevent persistence recognition. Runtime storage is bounded and may drop entries.

See [the operation guide and 30-minute hardware procedure](BLE_PRIVACY_WATCH.md)
and [feature provenance](FEATURE_PROVENANCE.md).

## Files changed specifically for Phase 4

Compared against the preserved local Phase 3 snapshot (not against the older
Git HEAD, which also includes still-uncommitted Phase 2/3 work):

- `ATTRIBUTIONS.md`
- `docs/BLE_PRIVACY_WATCH.md`
- `docs/FEATURE_PROVENANCE.md`
- `include/ble_privacy.h`
- `include/goblin_ble.h`
- `include/goblin_ble_ui.h`
- `include/guard_analyzer.h`
- `include/radio_coordinator.h`
- `platformio.ini`
- `scripts/run_host_tests.py`
- `src/ble_privacy.cpp`
- `src/ble_signatures.cpp`
- `src/goblin_ble.cpp`
- `src/goblin_ble_ui.cpp`
- `src/goblin_guard.cpp`
- `src/goblin_ui.cpp`
- `src/guard_analyzer.cpp`
- `src/main.cpp`
- `src/radio_coordinator.cpp`
- `src/security_snapshot.cpp`
- `src/wifi_snapshot.cpp`
- `tests/air_monitor_test.cpp`
- `tests/ble_privacy_test.cpp`
- `tests/guard_baseline_test.cpp`
- `docs/PHASE4_VALIDATION.md` (this report)
- `dist/ESP-Goblin-ES3C28P-v0.1.0-alpha.0.bin` (regenerated artifact)
