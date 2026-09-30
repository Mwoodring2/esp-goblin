# Board Support

## Tier 1 — physical development target

### `cyd40_hosyond`

Hosyond 4.0-inch CYD-style ESP32-32E board.

Initial profile:

| Function | Value |
|---|---|
| Display | ST7796S |
| Resolution | 320x480 |
| TFT MISO | GPIO 12 |
| TFT MOSI | GPIO 13 |
| TFT SCLK | GPIO 14 |
| TFT CS | GPIO 15 |
| TFT DC | GPIO 2 |
| Backlight | GPIO 27 |
| Touch | XPT2046 |
| Touch CS | GPIO 33 |
| SD CS | GPIO 5 |
| SD SCK | GPIO 18 |
| SD MISO | GPIO 19 |
| SD MOSI | GPIO 23 |

Display/radio bring-up should be verified before enabling touch and SD.

## Planned CYD families

- 2.4-inch classic ESP32 CYD
- 2.8-inch ESP32-2432S028R/C
- 3.2-inch CYD variants
- 3.5-inch ESP32-3248S035R/C
- 4.0-inch ESP32-32E ST7796S
- 4.0-inch ESP32-S3 square variants
- 4.3-inch ESP32-S3 variants
- 5.0-inch ESP32-S3 variants
- 7.0-inch ESP32-S3 variants

Support is profile-driven, not hard-coded into security modules.
