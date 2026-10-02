# Phase 3: passive management-frame air monitor

This is an original ESP Goblin receive-only monitor. It uses management-frame
metadata to count activity and flag unusually concentrated disconnect traffic.
It does not establish malicious intent or authenticate a frame's claimed sender.
Phase 2's local, uncommitted trusted baseline implementation was preserved.

## Controls

1. Boot and tap the hardware screen to reach the existing AP-scan/Guard summary.
2. Tap **AIR >** at the top right, then **START**.
3. AIR shows the current channel, approximate management frames/second, lifetime
   beacons, probe requests/responses, authentication, deauthentication,
   disassociation, queue occupancy/high-water, and dropped events.
4. Tap **CH n >** for per-channel packet activity counts/bars. These are observed
   packets, not RF energy, and are not normalized for time spent on each channel.
5. **GUARD <** returns to the existing trusted AP workflow. Monitoring continues.
6. **AP SCAN** pauses reception/hopping, performs the existing Wi-Fi discovery
   scan, merges inventory/Guard observations, and attempts to restore the previous
   monitor state even if scanning fails. Starting an AP scan is the explicit
   exception to passive listening: the existing discovery scan sends probes.
7. **ALERTS** opens observations. **STOP** disables reception/hopping without
   resetting lifetime counters; **START** resumes. Counters reset on reboot.

The main loop handles start/stop/scan actions. UI code does not perform driver
setup, packet parsing, NVS writes, or burst analysis. TFT refresh is at most once
every 750ms for live AIR/channel screens and pauses during a held touch. The
existing press/release handling prevents a held press from repeating actions.

## Separation and callback constraints

| Module | Responsibility |
|---|---|
| `WifiPromiscuousMonitor` | ESP32 driver start/stop, country channels, callback |
| `WifiFrameEvent` / parser | Fixed metadata and bounded header/fixed-field parsing |
| `WifiFrameQueue` | 128-slot single-producer/single-consumer queue |
| `AirMonitor` | Main-loop processing, lifetime/rolling/per-channel counters |
| `DeauthDetector` | Per-BSSID rolling disconnect windows |
| `GuardAnalyzer` | Existing baseline analysis plus bounded burst alert storage |
| `goblin_air` | Main-loop integration and rate-limited diagnostics |
| Goblin UI | Rendering and user action dispatch |

The Wi-Fi-task callback checks the buffer, management type and RX error state,
subtracts the four-byte FCS from ESP-IDF `sig_len`, calls the bounded parser,
copies one event, and returns. It never allocates, logs, calls TFT/Preferences,
searches AP inventories, parses information elements, waits, or analyzes alerts.

Queue slots contain 40-byte POD events: timestamp, subtype, source, destination,
BSSID, RSSI, channel, optional reason, and protected-body flag. No payload or
driver pointer is retained. The queue has 5,120 bytes of event storage, plus
fixed atomic indexes/counters. Acquire/release publication uses always-lock-free
32-bit atomics; startup checks compiler capability constants and refuses unsupported
targets. There are no callback mutexes, spin loops, allocations or unbounded loops.
Overflow drops the new event and increments a counter.

Queue reset is explicit and requires quiescent producer and consumer. Radio
pause/resume never resets queue storage, so an in-flight callback remains safe
and lifetime/drop diagnostics survive scanning. The main loop drains at most
64 events per service call, twice per loop, leaving time for touch/UI handling.

## Radio and parsing

Only `WIFI_PROMIS_FILTER_MASK_MGMT` is requested. No DATA, CTRL or FCS-failed
traffic is requested. After Arduino initializes the driver, listening uses
`WIFI_MODE_NULL`; there is no connected station or advertising soft AP. No
injection, deauth/disassociation transmission, spoofing, credential/handshake
collection, jamming, PCAP, GPS or web feature is introduced. The promiscuous
callback does not touch the SD card. Phase 5 may record a disconnect burst
later, from the main loop, as one JSONL event.

Hopping uses `esp_wifi_get_country()` start/count values, rechecked on transitions.
Only valid configured 2.4GHz ranges within channels 1–14 are accepted. An invalid
range or API failure stops/refuses monitoring and is logged with `esp_err_to_name`.
No country is guessed or overridden. Named settings in `include/air_config.h`:

| Setting | Default |
|---|---|
| Channel dwell | 300ms |
| Live UI refresh | 750ms |
| Serial diagnostic interval | 5 seconds |
| Burst window | 2,000ms |
| Burst threshold | 10 deauth/disassociation frames |
| Sustained burst notification cooldown | 10 seconds |
| Verbose channel/per-frame logs | Off |

The parser accepts version-0, normal, unfragmented management frames with zero
DS bits. It recognizes association/reassociation request/response, probe
request/response, beacon, authentication, disassociation, deauthentication and
action/action-no-ack. It validates the full 24-byte header and required fixed
body before accessing fields. Bounded optional HT Control is supported for
action frames only. Reserved formats, fragments, invalid channels, short headers
and truncated required bodies are rejected. Information elements are not parsed
or validated. Protected frames are counted but their bodies are never decoded
as plaintext reason codes; reason availability is explicit.

## Bursts and alerts

The detector groups solely by valid unicast BSSID, combining observed deauth and
disassociation frames in an inclusive two-second rolling timestamp window. No
SSID grouping is used. Frames include retransmissions; there is no deduplication
or cryptographic sender verification. Counts reflect successfully queued/processed
observations, not all traffic that may have existed over the air.

Tracking is bounded to 16 BSSIDs, each with 64 timestamps. An active-table eviction
is counted. If more than 64 matching frames occur in the window, the alert shows
`64+` until the discarded timestamps have expired. Counts cannot silently grow
without bounded storage. Frames with invalid/broadcast BSSIDs still contribute
to overall management counters but not to attributed AP bursts.

At the threshold, Guard records **UNUSUAL DISCONNECT ACTIVITY**, including BSSID,
last source MAC, channel, RSSI, count/window, optional last reason, and whether
the BSSID was trusted at observation time. Details show source/BSSID equality as
a fact, not evidence that the router sent the packet. Stored SSID is used only
as a display label; an unscanned AP is labeled with its BSSID/name unavailable.

Continuous events update one alert per BSSID. Dismissal survives updates until
a new episode crosses the threshold or a sustained episode reaches the named
notification cooldown. Up to eight burst alerts share the existing 32-alert
buffer, preserving space for Phase 2 SSID/security/channel observations. Oldest
burst history is replaced when full. Scan refresh preserves passive alerts.
SSID collision and disconnect observations remain separate; there is no
automatic conclusion that an Evil Twin or other attack occurred.

Rolling counters use ten 100ms buckets (an approximate trailing second); lifetime
and per-channel counts are independent. Counters continue to exist while stopped,
but the rolling rate ages to zero. Queue drops are tracked in both views.

## Diagnostics and limits

Every five seconds while monitoring (or after activity), serial reports current
and minimum observed free heap/PSRAM, queued/high-water/dropped/processed/rejected
counts, current channel, and detector evictions. Consecutive drop periods produce
a sustained-overflow warning after three reporting periods. Verbose mode logs
channel changes and up to four individual disconnect events per second, from
the main loop only. Burst messages are notification-limited, not per-frame spam.

There is no new PSRAM dependency. Queue, detector histories, rolling counters and
alerts use bounded ordinary RAM. Phase 2 AP inventory still grows on the heap
with checked allocations. Linker RAM usage excludes task stacks, driver buffers,
dynamic inventory and UI allocations. Minimum heap/PSRAM diagnostics are observed
samples, not a proof that all momentary allocation lows were captured.

This radio listens to one channel at a time. Hopping, manual scan pauses, UI/NVS
work, parser exclusions and queue overflow create blind spots. The initial dwell
and burst settings are provisional. PHY/RF behavior, radio restart, TFT/touch
responsiveness and long-run memory stability require real-device acceptance.
Host tests do not establish these hardware results.

## Validation and hardware acceptance

Run all hardware-independent suites with a C++11 compiler using:

```text
python scripts/run_host_tests.py
pio run -e hosyond_es3c28p
```

Set `CXX` to the host compiler executable. The runner can also use the project's
ignored Python/Zig environment. It runs existing inventory and baseline tests,
plus synthetic frame classification, reason/MAC extraction, protected/truncated/
invalid input, FIFO/overflow/reset, a 20,000-event concurrent queue exercise,
burst threshold/expiry/BSSID separation, saturated history, channel range/dwell/
clock wrap, scan pause/resume/failure handling, rolling/lifetime counters and
trusted alert/dismissal/capacity integration. No synthetic test traffic is sent
over the radio.

The output remains `dist/ESP-Goblin-ES3C28P-v0.1.0-alpha.0.bin`: merged ESP32-S3
firmware flashed at **0x0000**, with bootloader, partition table, boot_app0 and
application at the existing offsets. Preserve NVS to retain trusted baselines.

On the Hosyond ES3C28P:

1. Boot; confirm the display, FT6336G touch and existing Guard trust workflow.
2. Open AIR and press START. Confirm `monitor running` and the country range.
3. Watch the channel change, and beacon/probe counters increase in normal ambient
   traffic. Probe counters may remain zero in a quiet environment.
4. Open channel activity, return to AIR, and verify touch/UI responsiveness.
5. Press AP SCAN. Confirm stop/pause, scan completion, and automatic resume logs;
   verify counts/inventory/trust state and that hopping resumes.
6. Press STOP, run an AP scan, and confirm the monitor remains stopped; restart.
7. Run AIR for at least **15 minutes** without crash/reboot. Exercise the UI.
8. Record queue drops/high-water, processed/rejected frames, detector evictions,
   and minimum observed free heap/PSRAM from serial diagnostics.
9. Record any start/stop/channel API failures for investigation.

Do not generate deauth/disassociation traffic to test the detector. Synthetic
host tests validate its logic; real-world disconnects may be observed naturally.

## References and changed files

API/header behavior was checked against the installed ESP-IDF/Arduino framework
and [Espressif Wi-Fi API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html)
and [Espressif sniffer-mode documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi-driver/wifi-modes.html).
No other firmware's implementation was copied.

Phase 3 adds:

- `include/air_config.h`, `include/air_monitor.h`, `src/air_monitor.cpp`
- `include/channel_scheduler.h`
- `include/deauth_detector.h`, `src/deauth_detector.cpp`
- `include/goblin_air.h`, `src/goblin_air.cpp`
- `include/wifi_frame_event.h`, `src/wifi_frame_event.cpp`
- `include/wifi_frame_queue.h`
- `include/wifi_promiscuous_monitor.h`, `src/wifi_promiscuous_monitor.cpp`
- `tests/air_monitor_test.cpp`, `scripts/run_host_tests.py`, `docs/AIR_MONITOR.md`

Phase 3 modifies the existing/local Phase 2 files:

- `include/guard_analyzer.h`, `src/guard_analyzer.cpp`
- `include/goblin_ui.h`, `src/goblin_ui.cpp`, `src/main.cpp`
- `include/wifi_snapshot.h`, `src/wifi_snapshot.cpp`, `src/security_snapshot.cpp`
- `src/goblin_guard.cpp`, `docs/FEATURE_PROVENANCE.md`

Board pins, touch driver, PlatformIO configuration and merge script remain
unchanged. The pre-Phase-3 local implementation is also backed up in the ignored
`.pio/phase2-before-phase3.zip`. Existing Phase 2 uncommitted files remain in
the working tree; this patch does not discard or replace that baseline.
