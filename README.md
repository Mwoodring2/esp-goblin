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
- microSD event history (1-bit SD_MMC)
- RGB LED
- battery support
- audio hardware

## Current alpha

This build includes:

- board memory profile, display, backlight, and FT6336G touch
- flash and PSRAM diagnostics
- Wi-Fi AP discovery and a trusted access-point baseline
- passive Wi-Fi management-frame monitoring and disconnect bursts
- passive BLE Privacy Watch
- SD event history, review, and JSONL/CSV export
- one merged binary at offset `0x0000`

PCAP capture, GPS, wardriving, and a web dashboard are not part of this build.

## SD event history

ESP Goblin logs observations made by the device. It does not establish malicious
intent, identify an owner, or record a location.

With a microSD card installed, each boot appends one session file:

```text
/goblin/
  config/
  logs/
  sessions/session_000001.jsonl
  export/
  diagnostics/
```

The session number comes from a counter in NVS. It does not depend on the clock
or on Wi-Fi. `session_000042.jsonl` is the 42nd boot that obtained a session id.
The canonical record is that session file, one JSON object per line. A global
`events.jsonl` is not written.

Every event has `session`, `event`, and `mono_ms` (milliseconds since boot).
`time_valid` is false until the device actually has UTC time. Goblin does not
invent calendar timestamps and does not call `millis()` a Unix time. If UTC
becomes available during the boot, a `TIME_SYNC` event records the relationship.

On the HISTORY screen, events without UTC are shown as `+08m32s` from the
session start. Events with UTC are shown as `HH:MM UTC`.

Open STORAGE from the `SD OK` / `SD --` / `SD ERR` / `SD LOW` label in the
header. From there:

- HISTORY reviews the recent in-memory index
- EXPORT writes `/goblin/export/goblin_export_session_NNNNNN.jsonl` and `.csv`
- DIAG writes `/goblin/diagnostics/diagnostics.txt`
- the Logging row opens logging, summary, checksum, and retention switches

Copy the export files off the card with a normal card reader. JSONL is the
canonical log. CSV repeats the common columns and leaves irrelevant ones empty.
An optional `session_NNNNNN.sha256` file is an integrity checksum of the session
file. It is not a forensic chain of custody.

If the card is missing or a write fails, Guard, Air, and BLE keep running.
The header shows `SD ERR` or `SD --`. Use RETRY SD after reseating the card.
Events that could not be queued are counted as dropped and are not rewritten
as if they had been stored. Goblin does not reboot because the card failed.

Retention keeps at least 64 MB free when the card is large enough, and at most
100 session files. The current session and `/goblin/config/` are not deleted.
Cleanup uses the session number, not the wall clock. Age limits wait until a
real clock exists; they are not enforced in this build.

Logging defaults to on. Periodic AIR and BLE summaries default to every 5
minutes. Critical and warning events request a flush immediately; other events
flush after 8 records or 5 seconds. See
[docs/EVENT_HISTORY.md](docs/EVENT_HISTORY.md).

## Philosophy

ESP Goblin is built first as a defensive personal-network appliance:

- asset inventory
- new-device detection
- rogue-AP alerts
- deauth detection
- BLE/privacy monitoring
- SD event history
- authorized diagnostics

## License

GPL-3.0-or-later.

See `ATTRIBUTIONS.md` and `docs/FEATURE_PROVENANCE.md`.
