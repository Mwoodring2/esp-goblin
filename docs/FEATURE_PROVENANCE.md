# Feature Provenance

Every substantial feature must have an entry here before public release.

| Feature | Status | Implementation | Reference studied | Copied source | License impact |
|---|---|---|---|---|---|
| ES3C28P display bring-up | Alpha | ESP Goblin original | LCDWiki, Bruce board port | No | TFT_eSPI MIT |
| FT6336G touch | Alpha | ESP Goblin original | public FT6336 register behavior, Bruce board validation | No | None |
| Wi-Fi AP inventory | Alpha | ESP Goblin original using Arduino WiFi API | ESP32 ecosystem | No | Framework |
| Goblin Guard baseline | Planned | Original | Wireless/security-monitor concepts | No | None |
| BLE privacy detection | Planned | Original + approved open datasets | SquachWatch concept | No currently | TBD |
| Flock signature detection | Planned | TBD | flock-you | No currently | MIT if reused |
| Remote ID | Planned | Spec-based | ASTM/OpenDroneID | No currently | Apache/spec |
| BLE spam detection | Planned | TBD | Wall of Flippers | No currently | MIT if reused |
| Wi-Fi/BLE scheduler | Planned | Original measured scheduler | multiple public projects | No | None |
| Wardriving mesh | Planned | Original protocol unless compatible open source selected | Piglet concept only | No | Avoid NC code |

## Import rule

- MIT/BSD/Apache/public standards: may be incorporated with notices.
- GPL-compatible code: may be incorporated deliberately with provenance.
- Proprietary / non-commercial / unclear licensing: do not copy source.
