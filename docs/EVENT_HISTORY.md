# SD event history

Phase 5 gives ESP Goblin a durable session log. It records meaningful
observations. It does not capture packets, connect to BLE devices, or upload
anything.

ESP Goblin logs observations made by the device. A log entry does not establish
malicious intent, name an owner, or place the device on a map.

## Layout

```text
/goblin/config/          reserved, never cleaned up
/goblin/logs/            reserved; not a second copy of the event log
/goblin/sessions/session_000042.jsonl
/goblin/sessions/session_000042.sha256   optional integrity checksum
/goblin/export/goblin_export_session_000042.jsonl
/goblin/export/goblin_export_session_000042.csv
/goblin/diagnostics/diagnostics.txt
```

The session file is canonical. Export copies it. CSV is a flat view of the
common columns. Addresses that were actually observed may appear. Wi-Fi
passwords, credentials, cookies, and keys are not stored.

## Session id and time

NVS namespace `goblin_log`, key `next`, stores the next session number. If that
read fails, Goblin uses one past the highest `session_*.jsonl` id. Filenames do
not use the clock.

`mono_ms` is milliseconds since boot. `time_valid` is false unless the system
clock is a plausible UTC time. A later `TIME_SYNC` event is the only bridge
between monotonic time and UTC. History shows `+08m32s` or `HH:MM UTC`. It does
not invent a clock.

## What is logged

Boot and session start, reset reason, storage mount and write failures, Guard
alerts, trust and forget actions, dismissals, deauth and disassociation bursts,
Air and BLE summaries, radio transition failures, and low-memory warnings.

Beacons, probes, and ordinary BLE advertisements are counted in summaries. They
are not written one by one. Repeated warnings for the same identity are
deduplicated. User actions are not deduplicated.

## Limits

| Item | Value |
|---|---|
| Event queue | 64 records |
| Recent history | 100 records |
| JSON line buffer | 3072 bytes |
| Dedup slots | 48 |
| Flush interval | 5 seconds |
| Flush batch | 8 records, or immediately for warning/critical |
| Summary interval | 5 minutes |
| Free-space reserve | 64 MB, or 10% of a smaller card, whichever is lower |
| Session files | 100 maximum |
| SD mode | 1-bit SD_MMC at 20 MHz |

Pins come from `GoblinBoard` (`SD_CLK` GPIO38, `SD_CMD` GPIO40, `SD_D0` GPIO39).
D1, D2, and D3 remain on the board profile and are not part of this 1-bit bus.
4-bit mode waits until mount and repeated writes are stable on a physical ES3C28P.

The logger and history live in internal RAM: about 27 KB for the queue and
about 38 KB for the 100-record history. They are not placed in PSRAM.

If the queue fills, the new event is dropped, the drop counter increments, and
monitoring continues. Dropped events are not later described as stored.

Retention may delete the oldest completed session file, and its checksum, only
under `/goblin/sessions/`. The current session is never selected. The first
low-space warning does not delete files; a later retention pass may. Age is not
enforced until a real clock exists.

## Failure behavior

A missing or corrupt card logs a storage event, shows `SD --` or `SD ERR`, and
leaves Guard, Air, and BLE running. Goblin does not reboot for storage failure.
RETRY SD mounts again and writes `LOG_RECOVERY`. Lines that were only queued
can then be written. Lines that were dropped stay dropped.

An abrupt power loss leaves each finished JSONL line intact. On the next boot
Goblin does not rewrite the old file. If the tail is not a complete line, the
new session records `PREVIOUS_LOG_TRUNCATED`.

`SESSION_END` is written from the ESP-IDF shutdown handler on a software
restart. Pulling power does not produce that record.

## Manual acceptance on the Hosyond ES3C28P

This procedure was not run in the implementation session. Do not mark the board
verified from the host tests alone.

1. Insert a microSD card.
2. Boot ESP Goblin.
3. Confirm the header shows `SD OK` and serial reports a 1-bit mount.
4. Confirm `SESSION_START` is the first lines of `/goblin/sessions/session_NNNNNN.jsonl`.
5. Start AIR monitor and BLE Privacy Watch from the existing controls.
6. Let normal observations occur: AP discoveries, one trust or forget action, and ordinary BLE advertisements.
7. Open STORAGE from the header label, then HISTORY.
8. Confirm the events are listed. Without UTC they use `+mmss`, not a clock.
9. Open one event and confirm only the fields that exist are shown.
10. Reboot.
11. Confirm the session id increased.
12. Confirm the previous session file is still readable.
13. If the socket and filesystem allow it, remove the card while running.
14. Confirm the radio UI still responds and the label becomes a storage error.
15. Reinsert the card and use RETRY SD, or reboot with the card installed.
16. Confirm `LOG_RECOVERY` or a new successful mount. Do not expect dropped events to reappear.
17. EXPORT the current session.
18. On a PC, confirm every JSONL line parses as one JSON object.
19. Open the CSV. Empty columns are expected for fields that event did not have.
20. Run at least 60 minutes with monitoring on.
21. Confirm no spontaneous reboot and that touch still responds.
22. Record minimum free heap, minimum free PSRAM, event-queue drops, Wi-Fi queue drops, and BLE queue drops from DIAG or serial.

Abrupt power test:

1. Allow several events to flush.
2. Remove power without a clean shutdown.
3. Boot again.
4. Confirm earlier lines that ended in a newline still parse.
5. Confirm the new session starts and Goblin does not try to repair the old file.

Do not corrupt the card with a destructive test tool.

## Host coverage

`python scripts/run_host_tests.py` includes `event_log_test`. It covers JSONL
escaping, schema and ids, valid and invalid timestamps, `TIME_SYNC`, dedup and
cooldown, queue overflow and high water, history capacity, retention order,
path construction, traversal rejection, CSV escaping, truncated tails, low-space
threshold, storage-state transitions, SHA-256 vectors, the checksum wording,
and diagnostics text.
