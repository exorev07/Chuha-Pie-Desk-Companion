# Chuha Pie V1 - A cute Desk Companion

![Chuha Pie V1 Enclosure](assets/device.png)

![Platform](https://img.shields.io/badge/Platform-ESP32-blue?logo=espressif)
![Framework](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino)
![Language](https://img.shields.io/badge/Language-C%2B%2B-f34b7d?logo=c%2B%2B)
![Spotify](https://img.shields.io/badge/Spotify-API%20Integrated-1DB954?logo=spotify)
![License](https://img.shields.io/badge/License-MIT-green)

An ESP32-based animated desk companion with OLED eyes, touch interactions, Spotify controls, presence detection, and environmental monitoring — all housed in a custom 3D-printed enclosure.

---

## Table of Contents

- [Hardware](#hardware)
- [Pin Connections](#pin-connections)
- [Features](#features)
- [Libraries Required](#libraries-required)
- [Setup](#setup)
- [Power](#power)
- [Repository Structure](#repository-structure)
- [License](#license)

---

## Hardware

| Component | Model |
|---|---|
| Microcontroller | ESP32 DevKit V1 |
| Display | SSD1306 128×64 OLED (SPI, 7-pin) |
| Touch Sensor | TTP223 Capacitive Touch |
| Distance Sensor | HC-SR04 Ultrasonic |
| Temp/Humidity | DHT11 |
| Indicator | RGB LED (Common Anode, 10mm diffused) |
| Haptics | 5V Vibration Motor Module |
| Power | USB via ESP32 DevKit |
| Sleep Control | SPST Switch (deep sleep via GPIO35) |

---

## Pin Connections

### OLED Display (SPI)
| OLED Pin | ESP32 Pin |
|---|---|
| GND | GND |
| VDD | 3.3V |
| SCK | GPIO 18 (HSPI CLK) |
| SDA | GPIO 23 (HSPI MOSI) |
| RES | GPIO 4 |
| DC | GPIO 2 |
| CS | GPIO 5 |

### TTP223 Touch Sensor
| Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SIG | GPIO 15 |

### HC-SR04 Ultrasonic Sensor
| Pin | ESP32 Pin |
|---|---|
| VCC | VIN (5V) |
| GND | GND |
| TRIG | GPIO 26 |
| ECHO | Voltage divider → GPIO 27 |

> ⚠️ **HC-SR04 ECHO outputs 5V. ESP32 GPIO is 3.3V tolerant only.**
> Use a voltage divider on ECHO:
> `ECHO → 1kΩ → GPIO27 → 2kΩ → GND` (divides 5V to ~3.3V)

### DHT11
| Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO 14 |

### RGB LED (Common Anode)
| Pin | ESP32 Pin | Notes |
|---|---|---|
| Anode (longest) | 3.3V | — |
| Red | GPIO 25 | 100Ω resistor in series |
| Green | GPIO 32 | No resistor (Vf ~3.0–3.4V) |
| Blue | GPIO 33 | No resistor (Vf ~3.0–3.4V) |

### Vibration Motor (3-pin module)
| Pin | ESP32 Pin |
|---|---|
| VCC | VIN (5V) |
| GND | GND |
| SIG | GPIO 13 |

### Power Switch
```
GND → 10kΩ → GPIO 35 → [switch] → 3.3V
```
- Switch **open (OFF)**: GPIO 35 reads LOW → device enters deep sleep
- Switch **closed (ON)**: GPIO 35 reads HIGH → device runs normally

> GPIO 35 is input-only with no internal pull resistor; hence the external 10kΩ resistor is required.

---

## Features

See the **Product Manual** (included in repo) for a detailed flowchart describing the touch interaction flow.

### Touch Interactions (TTP223)
| Gesture | Action |
|---|---|
| Single tap | Enter Feature Cycle (Date & Time) |
| Double tap | Bonk animation |
| Triple tap | Settings |
| Long press (1s) | Cycle to next feature mode |
| Long press (2s+) | Pet the bot — happy animation |
| Long press (4s) | Return to Home Screen from any mode |

### Feature Cycle
**Date & Time → Stopwatch → Pomodoro → Spotify → Distance → Temperature → Humidity → Home**

Long press advances to the next mode. From any mode, a 4-second long press returns directly to Home.

| Mode | Tap | Double Tap | Triple Tap |
|---|---|---|---|
| Date & Time | Toggle 12h/24h | — | — |
| Stopwatch | Start / Pause | Reset | — |
| Pomodoro (select) | Cycle duration options | — | — |
| Pomodoro (running) | Pause / Resume | — | — |
| Spotify | Play / Pause | Next song | Previous song |

### Settings (Triple Tap from Home)
- **Brightness** — 5 levels: 5% → 25% → 50% → 75% → 100%
- **Presence Range** (long press from Brightness) — sets detection distance: 30 / 50 / 75 / 100 / 125 cm. Posture alert threshold adjusts proportionally (~40% of detection distance).

### Automatic / Background Features
| Feature | Trigger | Indicator |
|---|---|---|
| Presence detection | Someone enters detection range | Greeting animation |
| Tired eyes | No presence for 30+ seconds | Eye animation |
| Break reminder | 1 hour of continuous presence | Green LED + vibration |
| Posture alert | Too close for 3+ seconds | Red LED + vibration (1 min cooldown) |
| Water reminder | Every hour / 1 min after 30+ min absence | Blue LED + vibration |
| Sweat animation | Temperature > 35°C | Eye animation (10s every 5 min) |
| Auto brightness | Time of day | 100% 7AM–7PM, 50% otherwise |

---

## Libraries Required

Install via Arduino IDE → **Tools → Manage Libraries**:

| Library | Author |
|---|---|
| Adafruit SSD1306 | Adafruit |
| Adafruit GFX Library | Adafruit |
| DHT sensor library | Adafruit |
| FluxGarage RoboEyes | FluxGarage |

---

## Setup

See the **Setup page** of the Product Manual (included in repo) for full step-by-step instructions including Arduino IDE configuration, board setup, and firmware upload.

### Quick Credential Setup

1. Copy `Main/secrets.h.example` and rename it to `Main/secrets.h`
2. Fill in your WiFi SSID, password, and Spotify credentials
3. To get a Spotify Refresh Token, run:

   ```bash
   python spotify_get_refresh_token.py
   ```

> `secrets.h` is gitignored and will never be committed to the repository.

---

## Power

- Powered via USB through the ESP32 DevKit V1 onboard regulator
- The physical switch triggers ESP32 deep sleep — effectively turning the device off even with USB connected
- Deep sleep wake: flip switch to ON position (GPIO 35 goes HIGH)

---

## Repository Structure

```text
Chuha Pie V1/
├── Main/
│   ├── Main.ino                  # Main firmware
│   ├── secrets.h                 # Your credentials (gitignored, never committed)
│   └── secrets.h.example         # Template — copy and rename to secrets.h
├── assets/
│   └── device.png                # 3D enclosure render
├── spotify_get_refresh_token.py  # Helper script to generate Spotify refresh token
└── README.md
```

---

## License

MIT License — feel free to use, modify, and build upon this project.
