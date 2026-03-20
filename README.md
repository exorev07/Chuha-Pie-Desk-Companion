# Chuha Pie V1 — ESP32 Desk Companion

An ESP32-based animated desk companion with OLED eyes, touch controls, Spotify integration, and a suite of ambient awareness features.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 DevKit V1 |
| Display | SSD1306 128×64 OLED (7-pin SPI) |
| Touch Sensor | TTP223 Capacitive Touch |
| Distance Sensor | HC-SR04 Ultrasonic |
| Temp/Humidity | DHT11 |
| LED | 10mm RGB LED (Common Anode) |
| Haptics | 5V Vibration Motor Module (3-pin) |
| Power | USB via ESP32 Micro-USB |
| Switch | SPST toggle — triggers deep sleep |

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
| DC  | GPIO 2 |
| CS  | GPIO 5 |

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

> ⚠️ **ECHO outputs 5V — ESP32 GPIOs are 3.3V tolerant only.**
> Use a voltage divider on ECHO: `ECHO → 1kΩ → GPIO27 → 2kΩ → GND`

### DHT11
| Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO 14 |

### Vibration Motor (3-pin module)
| Pin | ESP32 Pin |
|---|---|
| VCC | VIN (5V) |
| GND | GND |
| SIG | GPIO 13 |

### RGB LED (Common Anode)
| Pin | ESP32 Pin |
|---|---|
| Anode (longest) | 3.3V |
| Red | 100Ω → GPIO 25 |
| Green | GPIO 32 |
| Blue | GPIO 33 |

### Power Switch
```
GND → 10kΩ → GPIO 35 → [switch] → 3.3V
```
- Switch **open** → GPIO 35 LOW → device enters deep sleep
- Switch **closed** → GPIO 35 HIGH → device runs normally

> Works even with USB plugged in. Deep sleep draws ~10–20µA.

---

## Power

- **Input:** 5V via USB (ESP32 Micro-USB)
- **Active current:** ~150–250mA typical (WiFi active), ~80–120mA (WiFi idle)
- **Peak current:** ~500mA+ during WiFi connection
- **Deep sleep:** ~10–20µA
- **No battery support** in this version — USB only

---

## Software Dependencies

Install via Arduino IDE Library Manager:

| Library | Author |
|---|---|
| Adafruit SSD1306 | Adafruit |
| Adafruit GFX Library | Adafruit |
| DHT sensor library | Adafruit |
| FluxGarage RoboEyes | FluxGarage |

ESP32 board support via Boards Manager:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Board: **ESP32 Dev Module** or **DOIT ESP32 DevKit V1**

---

## Configuration

Credentials are stored in `Main/secrets.h` which is gitignored and never pushed.
A template is provided at `Main/secrets.h.example` — copy it, rename to `secrets.h`, and fill in your values.

```cpp
#define WIFI_SSID_VAL             "your_wifi_name"
#define WIFI_PASSWORD_VAL         "your_wifi_password"
#define SPOTIFY_CLIENT_ID_VAL     "your_client_id"
#define SPOTIFY_CLIENT_SECRET_VAL "your_client_secret"
#define SPOTIFY_REFRESH_TOKEN_VAL "your_refresh_token"
```

For Spotify setup, see the Setup page of the product manual included in the repo.

---

## Features

### Touch Interactions (TTP223)
| Gesture | Action |
|---|---|
| Single tap | Enter Feature Cycle (Date/Time first) |
| Double tap | Bonk animation |
| Triple tap | Brightness settings |
| Long press (>2s) | Pet the bot — affection animation |
| Long press (>4s) | Return to Home Screen from any mode |

### Feature Cycle
Accessible via single tap from Home Screen. Long press advances to the next mode.

```
Date/Time → Stopwatch → Pomodoro → Spotify → Distance → Temperature → Humidity → Home
```

| Mode | Controls |
|---|---|
| Date/Time | Tap — toggle 12hr/24hr format |
| Stopwatch | Tap — start/pause, Double tap — reset |
| Pomodoro | Tap — cycle durations (30/45/60/90/120 min), auto-starts in 10s; Long press — back to selection |
| Spotify | Tap — play/pause, Double tap — next, Triple tap — previous |
| Distance / Temp / Humidity | Display only |

### Automatic / Ambient Features
| Feature | Behaviour |
|---|---|
| Presence detection | Greets on arrival; shows tired eyes after 30s of absence |
| Posture alert | Red LED + vibration if closer than 20cm for 3+ seconds (1 min cooldown) |
| Break reminder | Green LED + vibration after 1 hour of continuous presence |
| Water reminder | Blue LED + vibration every hour, and 1 min after returning from a 30+ min absence |
| Sweat animation | Eyes sweat when temperature exceeds 35°C (triggers every 5 min for 10s) |
| Auto brightness | 100% brightness 7AM–7PM, 50% at night. Requires NTP sync. Falls back to 75% if no WiFi. |

### Brightness Settings (Triple Tap)
Cycles through 5 levels: **5% → 25% → 50% → 75% → 100%**

---

## File Structure

```
Chuha Pie V1/
├── Main/
│   ├── Main.ino              # Primary firmware
│   ├── secrets.h             # Credentials (gitignored — not in repo)
│   └── secrets.h.example     # Template for credentials
├── Test/
│   └── Test.ino              # OLED display test (no WiFi)
├── SwitchTest/
│   └── SwitchTest.ino        # GPIO35 power switch debug sketch
├── spotify_get_refresh_token.py  # Helper script to get Spotify Refresh Token
└── README.md
```

---

## Notes

- WiFi is required for Spotify, NTP time sync, and auto-brightness. All other features work offline.
- Spotify controls the currently active device on your account — make sure something is playing on another device before switching to Spotify mode.
- NTP is configured for IST (GMT+5:30). To change timezone, update `GMT_OFFSET_SEC` in `Main.ino`.
- The power switch puts the ESP32 into deep sleep via `ext0` wakeup on GPIO 35 (RTC GPIO). An external 10kΩ resistor to GND is required since GPIO 35 has no internal pull resistor.
