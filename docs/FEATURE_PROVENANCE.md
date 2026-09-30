# Feature Provenance

Every substantial feature must have an entry here before public release.

| Feature | Status | Implementation | Reference studied | Copied source | License impact |
|---|---|---|---|---|---|
| ES3C28P display bring-up | Alpha | ESP Goblin original | LCDWiki, Bruce board port | No | TFT_eSPI MIT |
| FT6336G touch | Alpha | ESP Goblin original | public FT6336 register behavior, Bruce board validation | No | None |
| Goblin Guard AP inventory | Alpha | Original BSSID-keyed in-RAM records merged from Arduino WiFi scans; new-AP serial events and inventory UI counts | Existing project Arduino WiFi scan integration | No | Framework; no new dependency |
| Goblin Guard baseline | Planned | Original | Wireless/security-monitor concepts | No | None |
| BLE privacy detection | Planned | Original + approved open datasets | SquachWatch concept | No currently | TBD |
| Flock signature detection | Planned | TBD | flock-you | No currently | MIT if reused |
| Remote ID | Planned | Spec-based | ASTM/OpenDroneID | No currently | Apache/spec |
| BLE spam detection | Planned | TBD | Wall of Flippers | No currently | MIT if reused |
| Wi-Fi/BLE scheduler | Planned | Original measured scheduler | multiple public projects | No | None |
| Wardriving mesh | Planned | Original protocol unless compatible open source selected | Piglet concept only | No | Avoid NC code |

## Import rule

Goblin Guard retains BSSID, SSID, channel, RSSI, numeric framework auth mode,
first_seen/last_seen (64-bit monotonic milliseconds since boot), seen_count
(once per scan), and Known/Unknown state. Newly observed BSSIDs default to
Unknown. Matching SSIDs do not combine APs; repeat BSSIDs update the existing
record and preserve first_seen and state. Empty or failed scans retain records;
the new count resets each scan. Records remain until reboot, including APs no
longer visible. The board-independent inventory uses checked heap allocation;
allocation failures are logged and the screen reports an incomplete inventory.
`goblinApInventory().setKnown(bssid, true/false)` supports explicit classification;
this milestone adds no classification UI or automatic trust learning.
No SD persistence, raw/promiscuous capture, or disruption features are included.

- MIT/BSD/Apache/public standards: may be incorporated with notices.
- GPL-compatible code: may be incorporated deliberately with provenance.
- Proprietary / non-commercial / unclear licensing: do not copy source.
