# ESP Goblin — Attributions and Third-Party Notices

ESP Goblin is licensed under GPL-3.0-or-later.

This file distinguishes **code incorporated**, **data incorporated**, and
**reference implementations studied**.

## Current incorporated dependencies

### TFT_eSPI
- Project: Bodmer/TFT_eSPI
- License: MIT
- Use: display driver abstraction for the first ES3C28P bring-up.

### Arduino-ESP32 / pioarduino
- Project: Espressif Arduino core / pioarduino platform packaging.
- Use: ESP32-S3 framework and build toolchain.
- License: see upstream component notices.

## Reference implementations studied — no source copied into ESP Goblin

### Bruce firmware
- Project: BruceDevices/firmware
- Purpose studied:
  - ES3C28P board memory configuration
  - hardware pin validation
  - display driver selection
  - FT6336G behavior
  - SD_MMC architecture
- ESP Goblin implementation is intentionally small and independently written.

### Wireless Wizard
- Purpose studied:
  - feature discovery
  - public firmware packaging model
  - attribution/provenance discipline
- No Wireless Wizard source is included in this starter.

### SquachWatch-CYD
- Purpose studied:
  - defensive device-signature concepts
  - passive monitoring UX
  - board-profile strategy
- No SquachWatch source is included in this starter.

## Restricted / clean-room references

ESP Goblin must not copy source from projects with proprietary, non-commercial,
or otherwise incompatible licensing. Features inspired by such projects must be
implemented independently from public protocol/hardware documentation and our
own testing.

See `docs/FEATURE_PROVENANCE.md`.

## Phase 4 incorporated dependency

### NimBLE-Arduino 2.5.1
- Project: [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino/tree/2.5.1).
- Exact PlatformIO dependency: `h2zero/NimBLE-Arduino@2.5.1`.
- License: [Apache-2.0](https://github.com/h2zero/NimBLE-Arduino/blob/2.5.1/LICENSE),
  including upstream component notices distributed with the dependency.
- Use: ESP32-S3 passive BLE scanning with NimBLEScanCallbacks, duplicate
  observations and callbacks-only results. The dependency is unmodified.
- The existing pioarduino platform and board/display/touch configuration are retained.

## Phase 4 specification and factual-data references

- [Bluetooth SIG Assigned Numbers](https://www.bluetooth.com/specifications/assigned-numbers/):
  standard advertising-data type identifiers and the single built-in company
  mapping `0x004C = Apple, Inc.`. Only these factual identifiers are used;
  no complete database or SIG document is copied. SIG material retains its
  published terms; it is not relabeled as Apache-licensed data. Other company
  IDs appear numerically. The lookup API can later accept another data backend.
- [Google Find Hub Network Accessory Specification](https://developers.google.com/nearby/fast-pair/specifications/extensions/fmdn):
  reference for advertised-frame structure and address/identifier rotation.
  Google documentation is CC BY 4.0 unless otherwise noted; code samples are
  Apache-2.0. No sample code or cryptographic implementation was incorporated.
- [Google Eddystone](https://github.com/google/eddystone): public UID, URL,
  plain/encrypted TLM and EID frame specifications, Apache-2.0. Matchers were
  written independently from the documented layouts, not copied source.

All Phase 4 parser, classifier, inventory, persistence, scheduling, queue and UI
logic is original ESP Goblin code under the project's GPL-3.0-or-later license.
No tracker signatures or implementation code were imported from Wireless Wizard,
HaleHound, Piglet, Bruce, Marauder, GhostESP or other firmware. Future imported
signatures must update this file and `docs/FEATURE_PROVENANCE.md` with the precise
source and compatible license before incorporation.

## Phase 5 storage

- Arduino-ESP32 `SD_MMC` and `Preferences`/NVS, from the existing Espressif
  Arduino core. No additional library was added for the filesystem or the log.
- SHA-256 in `src/sha256_goblin.cpp` is an original implementation of FIPS 180-4
  for optional integrity checksum files. It is not described as tamper-proof.
- No logging, export, or SD driver code was copied from Wireless Wizard, Bruce,
  Marauder, GhostESP, HaleHound, Piglet, or other firmware.
