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
