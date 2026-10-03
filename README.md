# SETU-V2

**Bridging communication gaps with a LoRa-based offline mesh for rescue, warehouse, and field operations.**

SETU-V2 is the upgraded version of SETU, moving from ESP-NOW to LoRa for longer range, multi-node communication, structured alerts, and a local live dashboard — all built on ESP32 + SX1278 hardware.

---

## What It Does

SETU-V2 is an offline mesh communication system designed for environments where cellular, Wi-Fi, or internet connectivity is unavailable or unreliable — disaster zones, remote warehouses, field operations, and similar scenarios.

Each node is a self-contained device with an OLED display, buttons, status LEDs, and a LoRa radio. Users can:

- **Select a use-case mode** at boot (Rescue, Warehouse, Field Operations)
- **Scroll through and send context-specific signals** within that mode
- **Receive and display incoming alerts** from other nodes in real time
- **View a live dashboard** (on designated host nodes) aggregating all mesh activity via a locally hosted web page

## What Makes It Different

- **Gateway-free architecture** — no dedicated gateway hardware required; any node can host the dashboard, and the mesh keeps functioning if that node goes offline
- **Multi-mode, multi-use** — one device serves multiple scenarios (disaster response, warehouse coordination, field operations) rather than being locked to a single use case
- **Structured, severity-aware signals** — fixed, icon-friendly signal categories instead of free-text messaging, designed for zero-training, language-agnostic operation
- **Low-cost, low-power hardware** — built on commodity ESP32 + SX1278 LoRa modules, making large-scale deployment realistic

## Modes & Signals

| Mode | Signals |
|---|---|
| **RESCUE** | SOS, FIRE, MEDICAL, TRAPPED, SAFE, WATER |
| **WAREHOUSE** | NEED_HELP, LOW_STOCK, DAMAGE, BREAK_TIME, TASK_DONE, EMERGENCY |
| **FIELD_OPS** | ARRIVED, DELAYED, ALL_CLEAR, NEED_BACKUP, MOVING, STANDBY |

## Hardware

- ESP32-S3 (dashboard host + mesh nodes)
- ESP32-C3 SuperMini (lightweight mesh nodes)
- SX1278 (Ra-02) LoRa modules, 433 MHz
- 0.96" I2C OLED displays
- Push buttons, status LEDs, buzzer

## How It Works

1. **Boot:** Each node shows a "Select Mode" screen
2. **Choose Mode:** Short-press SCROLL to cycle modes, press SEND to confirm
3. **Choose Signal:** Short-press SCROLL to cycle signals within that mode, press SEND to transmit
4. **Transmit:** Packet is sent over LoRa as `nodeId:MODE:SIGNAL` (e.g., `1:RESCUE:SOS`)
5. **Receive:** Other nodes display the incoming alert on their OLED, flash a status LED, and log it
6. **Dashboard:** The designated host node serves a live web page aggregating all mesh activity

## Dashboard

The dashboard-hosting node creates its own Wi-Fi hotspot (`RescueMesh_Dashboard`). Connect any phone or laptop to it and open `192.168.4.1` in a browser to view a live, color-coded table of all alerts across the mesh — no internet or router required.

## Upgrading from SETU (ESP-NOW → LoRa)

The original SETU used ESP-NOW for short-range, point-to-point alert communication. SETU-V2 upgrades this to LoRa mesh networking for:

- **Longer range** (multi-kilometer potential vs. ~150-500m for ESP-NOW)
- **True multi-node mesh** (not just point-to-point)
- **Gateway-free architecture** (no dedicated hub required)
- **Multi-mode flexibility** (not locked to a single use case)

## Libraries Used

- [RadioLib](https://github.com/jgromes/RadioLib) — LoRa communication
- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306) — OLED display
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) — graphics for OLED
- ESPAsyncWebServer — dashboard web server

## Status

Active development — core bidirectional LoRa communication and multi-mode menu system working; mesh routing and dashboard aggregation in progress.

## License

[Add your license here — e.g., MIT, GPL, etc.]
