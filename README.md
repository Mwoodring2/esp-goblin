# ESP Goblin

A CYD-first personal network-security monitor for ESP32 touchscreen hardware.

**Current build:** `v0.1.0-alpha.0`

## Reference Board A

Hosyond / QDtech ES3C28P:

- ESP32-S3
- 16 MB flash
- 8 MB OPI PSRAM
- 2.8" 240x320 ILI9341V IPS
- FT6336G capacitive touch
- microSD (future alpha milestone)
- RGB LED
- battery support
- audio hardware

## Current alpha

This first build validates:

- exact board memory profile
- display
- backlight
- FT6336G touch detection
- flash/PSRAM diagnostics
- Wi-Fi AP discovery
- open-network count
- channel congestion snapshot
- single merged binary generation

## Philosophy

ESP Goblin is built first as a defensive personal-network appliance:

- asset inventory
- new-device detection
- rogue-AP alerts
- deauth detection
- BLE/privacy monitoring
- PCAP capture
- SD event history
- authorized diagnostics

## License

GPL-3.0-or-later.

See `ATTRIBUTIONS.md` and `docs/FEATURE_PROVENANCE.md`.
