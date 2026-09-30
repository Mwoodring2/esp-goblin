# ESP Goblin Architecture

## Principle

Application features must not know the physical CYD pinout.

Hardware support is split into board profiles so the security engine can run across classic ESP32 and ESP32-S3 CYD variants.

## Planned layers

```text
Security / monitoring features
        |
Goblin services
        |
Hardware abstraction
        |
Board profile
        |
CYD hardware
```

## Core modules

Planned:

- Board registry
- Display abstraction
- Touch abstraction
- Storage abstraction
- Wi-Fi monitor
- BLE monitor
- Device inventory
- Baseline / change detector
- Alert engine
- Event timeline
- PCAP recorder
- LAN Sentry
- Web dashboard
- GPS / wardrive
- Optional ESP32-C5 Scout

## First development rule

Every new board should require a new board profile and, where needed, a thin hardware adapter.

Security logic should not gain board-specific pin checks.
