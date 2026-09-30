# Build and Flash — Hosyond ES3C28P

Project root:

`C:\Users\punke\Documents\ChatGPT\esp Goblin`

## Build

```powershell
cd "C:\Users\punke\Documents\ChatGPT\esp Goblin"
pio run -e hosyond_es3c28p
```

On a successful build, ESP Goblin also attempts to create:

```text
dist\ESP-Goblin-ES3C28P-v0.1.0-alpha.0.bin
```

That is the **merged single-file image**.

## Flash with ESP Terminator

Use:

```text
Chip: ESP32-S3
File: ESP-Goblin-ES3C28P-v0.1.0-alpha.0.bin
Offset: 0x0000
Erase: Yes for the first Goblin flash
```

## First expected screen

1. `ESP GOBLIN`
2. `WAKING THE GOBLIN...`
3. Hardware report
4. Tap the screen
5. `AIR SNIFFER` performs a normal Wi-Fi discovery scan

This alpha does not transmit attacks or disruptive frames.
