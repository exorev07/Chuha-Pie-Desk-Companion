# Chuha Pie V1 - A cute Desk Companion

<p align="center">
  <img src="assets/device.png" alt="Chuha Pie V1 Enclosure" width="500"/>
</p>

An ESP32-based animated desk companion with OLED eyes, touch interactions, Spotify controls, presence detection, and environmental monitoring — all housed in a custom 3D-printed enclosure.

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
See the **Product Manual** (included in repo) for detailed flowchart describing the touch interactionflow.

### Touch Interactions (TTP223)
| Gesture | Action |
|---|---|
| Single tap | Enter Feature Cycle (Date & Time) |
| Double tap | Bonk animation |
| Triple tap | Settings |
| Long press (1s) | Cycle to next feature mode |
| Long press (2s+) | Pet the bot - happy animation |
| Long press (4s) | Return to Home Screen from any mode |

### Feature Cycle
**Date & Time → Stopwatch → Pomodoro → Spotify → Distance → Temperature → Humidity → Home**

Long press advances to the next mode. From any mode, 4-second long press returns directly to Home.

| Mode | Tap | Double Tap | Triple Tap |
|---|---|---|---|
| Date & Time | Toggle 12h/24h | — | — |
| Stopwatch | Start / Pause | Reset | — |
| Pomodoro (select) | Cycle duration options | — | — |
| Pomodoro (running) | Pause / Resume | — | — |
| Spotify | Play / Pause | Next song | Previous song |

### Settings (Triple Tap from Home)
- **Brightness** — 5 levels: 5% → 25% → 50% → 75% → 100%
- **Presence Range** (long press from Brightness) — sets detection distance: 30 / 50 / 75 / 100 / 125 cm. Also adjusts posture alert threshold proportionally (~40% of detection distance).

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

- `Adafruit SSD1306`
- `Adafruit GFX Library`
- `DHT sensor library` by Adafruit
- `FluxGarage RoboEyes`

---

## Setup

See the **Setup page** of the Product Manual (included in repo) for full step-by-step instructions including:
- Arduino IDE and board configuration
- WiFi and Spotify credential setup
- Uploading firmware

### Quick credential setup
1. Rename Main/example_secrets.h to `Main/secrets.h`
2. Fill in your WiFi SSID, password, and Spotify credentials
3. To get a Spotify Refresh Token, run:
   ```
   python spotify_get_refresh_token.py
   ```

---

## Power

- Powered via USB through the ESP32 DevKit V1 onboard regulator
- The physical switch triggers ESP32 deep sleep — effectively turning the device off even with USB connected
- Deep sleep wake: flip switch to ON position (GPIO 35 goes HIGH)

---
