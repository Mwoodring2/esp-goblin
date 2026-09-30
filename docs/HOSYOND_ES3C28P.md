# Reference Board A — Hosyond ES3C28P

Primary ESP Goblin development/reference board.

## Hardware

- ESP32-S3 N16R8
- 16 MB flash
- 8 MB OPI PSRAM
- 2.8" 240x320 IPS
- ILI9341V display
- FT6336G capacitive touch
- microSD using 4-bit SDIO
- ES8311 audio codec
- onboard microphone
- external speaker connector
- WS2812-style single-wire RGB status LED
- battery input + charge management
- battery ADC

## Pin map

| Function | GPIO |
|---|---:|
| LCD CS | 10 |
| LCD DC | 46 |
| LCD SCLK | 12 |
| LCD MOSI | 11 |
| LCD MISO | 13 |
| LCD BL | 45 |
| Touch SDA | 16 |
| Touch SCL | 15 |
| Touch RST | 18 |
| Touch INT | 17 |
| SD CLK | 38 |
| SD CMD | 40 |
| SD D0 | 39 |
| SD D1 | 41 |
| SD D2 | 48 |
| SD D3 | 47 |
| RGB | 42 |
| Audio amp enable | 1 |
| I2S MCLK | 4 |
| I2S BCLK | 5 |
| I2S DOUT | 8 |
| I2S WS | 7 |
| I2S DIN | 6 |
| Battery ADC | 9 |

## Bring-up order

1. USB serial + hardware probe
2. Verify 16 MB flash / 8 MB PSRAM
3. LCD init and inversion
4. Touch I2C probe at 0x38
5. Touch coordinates / rotation
6. SD_MMC
7. RGB status LED
8. Battery ADC
9. Audio
10. Wi-Fi/BLE security monitor

Do not mark this board as public-flasher `verified: true` until the physical unit passes all required checks.
