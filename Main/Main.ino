/*
 * Chuha Pie V1 - Desk Companion
 * 
 * Hardware:
 * - ESP32 DevKit V1
 * - 7-Pin SSD1306 OLED (128x64, SPI)
 * - TTP223 Capacitive Touch Sensor
 * - HC-SR04 Ultrasonic Distance Sensor
 * - DHT11 Temperature & Humidity Sensor
 * - Vibration Motor (5V, GPIO-driven)
 * - RGB LED (Common Anode, 10mm diffused)
 * 
 * Pin Connections:
 * OLED GND  -> ESP32 GND
 * OLED VDD  -> ESP32 3.3V
 * OLED SCK  -> ESP32 GPIO 18 (HSPI CLK)
 * OLED SDA  -> ESP32 GPIO 23 (HSPI MOSI)
 * OLED RES  -> ESP32 GPIO 4 (Reset)
 * OLED DC   -> ESP32 GPIO 2 (Data/Command)
 * OLED CS   -> ESP32 GPIO 5 (Chip Select)
 * 
 * TTP223 VCC -> ESP32 3.3V
 * TTP223 GND -> ESP32 GND
 * TTP223 SIG -> ESP32 GPIO 15
 * 
 * HC-SR04 VCC  -> ESP32 VIN or 5V pin
 * HC-SR04 GND  -> ESP32 GND
 * HC-SR04 TRIG -> ESP32 GPIO 26
 * HC-SR04 ECHO -> VOLTAGE DIVIDER -> ESP32 GPIO 27
 * 
 * ⚠️ IMPORTANT: HC-SR04 ECHO outputs 5V but ESP32 GPIO is 3.3V tolerant!
 * Use voltage divider on ECHO pin:
 *   ECHO -> 1kΩ resistor -> GPIO 27 -> 2kΩ resistor -> GND
 *   This divides 5V to ~3.3V (safe for ESP32)
 * 
 * DHT11 VCC  -> ESP32 3.3V
 * DHT11 GND  -> ESP32 GND
 * DHT11 DATA -> ESP32 GPIO 14
 * 
 * Vibration Motor VCC -> ESP32 VIN (5V)
 * Vibration Motor GND -> ESP32 GND
 * Vibration Motor SIG -> ESP32 GPIO 13
 * 
 * Power Switch:
 * GND -> 10kΩ -> GPIO 35 -> [switch] -> 3.3V
 * Switch open (OFF): GPIO35 LOW -> device enters deep sleep
 * Switch closed (ON): GPIO35 HIGH -> device runs
 *
 * RGB LED (Common Anode, 10mm diffused):
 * RGB LED Anode (longest pin) -> ESP32 3.3V
 * RGB LED Red   -> 100Ω resistor -> ESP32 GPIO 25
 * RGB LED Green -> ESP32 GPIO 32 (no resistor needed, Vf ~3.0-3.4V)
 * RGB LED Blue  -> ESP32 GPIO 33 (no resistor needed, Vf ~3.0-3.4V)
 * 
 * Features:
 * - Touch interactions: single tap (mode enter), double tap (bonk), triple tap (brightness)
 * - Long press (>2s): Happy mood with laugh animation + haptic feedback
 * - 1s long press: Cycles through modes (Time > Stopwatch > Pomodoro > Spotify > Distance > Temp > Humidity)
 * - 4s long press: Home shortcut (return to eyes from any display mode)
 * - Time display with 12hr/24hr toggle (tap to switch)
 * - Stopwatch with start/pause (tap) and reset (double tap)
 * - Pomodoro timer (30/45/60/90/120 min, pause/resume, 10s auto-start)
 * - Spotify control: play/pause (tap), next (double tap), prev (triple tap)
 * - Distance, temperature, humidity displays
 * - Brightness adjustment: 5 levels via 3 SSD1306 registers (triple tap from eyes)
 * - Time-based default brightness: 100% daytime (7AM-7PM), 50% nighttime
 * - Presence detection: greets on arrival, shows TIRED after 30s absence
 * - Break reminder: hourly while present (green LED)
 * - Posture alert: too close (<20cm for 3s) with 1min cooldown (red LED)
 * - Water reminder: hourly + 1min after return from 30min absence (blue LED)
 * - RGB LED pulses with vibration for 2s, then solid for 3s during alerts
 * - Sweat animation above 35°C (10s every 5min)
 * - Auto blink and idle eye movements
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "base64.h"
#include <time.h>
#include <DHT.h>
#include <FluxGarage_RoboEyes.h>
#include "secrets.h"

// ============ WIFI CREDENTIALS ============
const char* WIFI_SSID = WIFI_SSID_VAL;
const char* WIFI_PASSWORD = WIFI_PASSWORD_VAL;

// ============ SPOTIFY CREDENTIALS ============
// Edit credentials in secrets.h (that file is gitignored and never pushed)
const char* SPOTIFY_CLIENT_ID = SPOTIFY_CLIENT_ID_VAL;
const char* SPOTIFY_CLIENT_SECRET = SPOTIFY_CLIENT_SECRET_VAL;
const char* SPOTIFY_REFRESH_TOKEN = SPOTIFY_REFRESH_TOKEN_VAL;

// ============ TIME CONFIGURATION ============
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 19800;  // IST is GMT+5:30 (5.5 * 3600 = 19800 seconds)
const int DAYLIGHT_OFFSET_SEC = 0;   // India doesn't use daylight saving

// ============ PIN DEFINITIONS ============
#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_DC     2
#define OLED_CS     5
#define OLED_RESET  4
#define TOUCH_PIN   15
#define TRIG_PIN    26
#define ECHO_PIN    27 
#define DHT_PIN     14 
#define VIBRATION_PIN 13
#define LED_RED_PIN   25
#define LED_GREEN_PIN 32
#define LED_BLUE_PIN  33
#define POWER_SWITCH_PIN 35  // Input-only RTC GPIO, external 10kΩ pulldown to GND, switch to 3.3V

// RGB LED PWM channels (common anode: 0=full on, 255=off)
#define LED_RED_CH    0
#define LED_GREEN_CH  1
#define LED_BLUE_CH   2

// ============ DHT11 SENSOR CONFIG ============
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ============ TEMPERATURE MONITORING ============
#define TEMP_CHECK_INTERVAL 10000   // Check temperature every 10 seconds
#define SWEAT_THRESHOLD 35.0        // Sweating animation above 35°C
#define SWEAT_INTERVAL 300000UL     // Sweat every 5 minutes
#define SWEAT_DURATION 10000        // Sweat for 10 seconds
unsigned long lastTempCheck = 0;
unsigned long lastSweatStart = 0;    // When last sweat episode started
float lastTemperature = 0;
bool isSweating = false;

// ============ DISTANCE SENSOR CONFIG ============
#define DISTANCE_CHECK_INTERVAL 500  // Check distance every 500ms
float smoothedDistance = 0;           // EMA-smoothed distance for display
bool smoothedDistanceInit = false;    // Whether EMA has been seeded
#define EMA_ALPHA 0.3                 // EMA weight (0.1=very smooth, 0.5=responsive)

// ============ DISTANCE SETTINGS ============
// Each index pairs a presence detection range with a posture alert threshold
const int detectionOptions[] = {30, 50, 75, 100, 125};  // cm — how far away to detect presence
const int postureOptions[]   = {15, 20, 30,  40,  50};  // cm — too-close posture threshold per level
int distanceSelected = 1;  // Default: 50cm detection / 20cm posture (matches original values)

// ============ DISPLAY SETUP ============
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define MAX_FPS       30

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, 
                         OLED_DC, OLED_RESET, OLED_CS);
RoboEyes<Adafruit_SSD1306> eyes(display);

// ============ TOUCH STATE ============
bool lastTouchState = false;
unsigned long touchStartTime = 0;
unsigned long touchDuration = 0;
unsigned long lastTapTime = 0;
bool isLongPress = false;
bool homePressTriggered = false;           // Tracks 4s "home" long press (back to face)

// ============ PRESENCE DETECTION ============
bool personPresent = false;
bool wasPersonPresent = false;
unsigned long lastDistanceCheck = 0;
unsigned long greetingStartTime = 0;
unsigned long lastDistanceDisplayUpdate = 0;
bool rawPresenceDetected = false;           // Raw sensor reading
unsigned long presenceChangeTime = 0;       // When raw detection last changed
#define PRESENCE_GONE_DELAY 5000            // 5s before marking absent
#define PRESENCE_ARRIVE_DELAY 50           // 50ms before marking present
enum GreetingState {
  NO_GREETING,
  GREETING_CURIOUS,
  GREETING_HAPPY,
  GREETING_DONE
};
GreetingState greetingState = NO_GREETING;

// ============ BREAK REMINDER ============
unsigned long continuousPresenceStart = 0;   // When person was first detected
bool breakReminderShown = false;             // Prevent repeated reminders
#define BREAK_REMINDER_INTERVAL 3600000UL    // 1 hour in ms

// ============ POSTURE ALERT ============
#define POSTURE_SUSTAIN_TIME 3000         // Must stay too close for 3s before alert
#define POSTURE_COOLDOWN 60000UL          // Don't re-alert within 1 minute
bool tooCloseDetected = false;             // Raw: currently under threshold
unsigned long tooCloseStartTime = 0;       // When too-close was first detected
unsigned long lastPostureAlert = 0;        // When last posture alert was shown
bool postureAlertActive = false;           // Alert already fired for this episode

// ============ WATER REMINDER ============
#define WATER_REMINDER_INTERVAL 3600000UL  // Remind every 1 hour
#define WATER_RETURN_DELAY 60000UL         // 1 min after returning from long absence
#define LONG_ABSENCE_THRESHOLD 1800000UL   // 30 min counts as "long absence"
unsigned long lastWaterReminder = 0;        // When last water reminder was shown
unsigned long lastPresenceEnd = 0;          // When person last left (for absence tracking)
bool waterReturnReminderPending = false;    // Waiting to show return reminder
unsigned long waterReturnDetectTime = 0;    // When person returned after long absence

// ============ ANIMATION STATE ============
enum State {
  IDLE,
  LONG_PRESS_BUILDING,
  SHOWING_AFFECTION,
  WAITING_FOR_SECOND_TAP,
  WAITING_FOR_THIRD_TAP,
  SHOWING_TIME,
  STOPWATCH_MODE,
  POMODORO_SELECT,
  POMODORO_RUNNING,
  POMODORO_DONE,
  SPOTIFY_MODE,
  SHOWING_DISTANCE,
  SHOWING_TEMPERATURE,
  SHOWING_HUMIDITY,
  BRIGHTNESS_ADJUST,
  DISTANCE_ADJUST,
  BONKED,
  RECOVERING,
  BREAK_REMINDER,
  POSTURE_ALERT,
  WATER_REMINDER
};

State currentState = IDLE;
State previousState = IDLE;            // For returning after alerts (break/posture)
unsigned long stateStartTime = 0;

// ============ BRIGHTNESS CONFIG ============
const int brightnessOptions[] = {5, 25, 50, 75, 100};  // Percentage values
// Each level: {contrast, precharge, vcomh} - all three registers for wide range
const byte brightnessContrast[] =  {  5,  30, 100, 170, 255};
const byte brightnessPrecharge[] = {0x00, 0x11, 0x22, 0xF1, 0xF1}; // Phase1|Phase2
const byte brightnessVCOMH[] =     {0x00, 0x10, 0x20, 0x30, 0x40}; // VCOMH deselect level
int brightnessSelected = 2;  // Default: 50% (index 2)

// ============ POMODORO CONFIG ============
const int pomodoroOptions[] = {30, 45, 60, 90, 120};  // Minutes
const char* pomodoroLabels[] = {"30 Min", "45 Min", "1 Hr", "1.5 Hr", "2 Hr"};
int pomodoroSelected = 0;               // Currently highlighted option (0-4)
unsigned long pomodoroSelectionTime = 0; // When current option was selected
unsigned long pomodoroEndTime = 0;       // When the timer should end
bool pomodoroPaused = false;             // Whether the timer is paused
unsigned long pomodoroPausedRemaining = 0; // Remaining time when paused (ms)

// ============ STOPWATCH STATE ============
bool time24HourFormat = false;               // Toggle between 24hr and 12hr display
bool stopwatchRunning = false;
unsigned long stopwatchStartTime = 0;        // When stopwatch was started/resumed
unsigned long stopwatchElapsed = 0;           // Accumulated elapsed time in ms
unsigned long stopwatchLastDoubleTap = 0;     // For double-tap reset detection
bool stopwatchWaitingSecondTap = false;       // Waiting for potential second tap

// ============ SPOTIFY STATE ============
String spotifyAccessToken = "";
unsigned long spotifyTokenExpiry = 0;       // When current token expires
String spotifyTrackName = "";
String spotifyArtistName = "";
bool spotifyIsPlaying = false;
unsigned long spotifyProgressMs = 0;        // Current position in ms
unsigned long spotifyDurationMs = 0;        // Track duration in ms
unsigned long spotifyLastProgressUpdate = 0; // When progress was last fetched from API
unsigned long lastSpotifyPoll = 0;
#define SPOTIFY_POLL_INTERVAL 3000           // Poll every 3 seconds (progress interpolated between polls)
int spotifyTapCount = 0;                     // Track taps for single/double/triple
unsigned long spotifyLastTapTime = 0;        // When last tap happened in Spotify mode
#define SPOTIFY_TAP_WINDOW 700               // ms to wait for more taps
bool spotifyTapPending = false;              // Whether we're waiting for more taps

// ============ SETUP ============

// Helper to apply brightness registers from brightnessSelected index
void applyBrightness() {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(brightnessContrast[brightnessSelected]);
  display.ssd1306_command(SSD1306_SETPRECHARGE);
  display.ssd1306_command(brightnessPrecharge[brightnessSelected]);
  display.ssd1306_command(SSD1306_SETVCOMDETECT);
  display.ssd1306_command(brightnessVCOMH[brightnessSelected]);
}

void setup() {
  Serial.begin(115200);

  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    for(;;);
  }
  display.clearDisplay();
  display.display();

  // Initialize outputs required for a clean goToSleep() shutdown
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);
  ledcSetup(LED_RED_CH, 5000, 8);
  ledcSetup(LED_GREEN_CH, 5000, 8);
  ledcSetup(LED_BLUE_CH, 5000, 8);
  ledcAttachPin(LED_RED_PIN, LED_RED_CH);
  ledcAttachPin(LED_GREEN_PIN, LED_GREEN_CH);
  ledcAttachPin(LED_BLUE_PIN, LED_BLUE_CH);
  ledOff();

  // Power switch: check BEFORE expensive boot (WiFi, animations)
  // If OFF at power-on, sleep immediately without running startup sequence
  pinMode(POWER_SWITCH_PIN, INPUT);
  if (digitalRead(POWER_SWITCH_PIN) == LOW) goToSleep();

  // --- Switch is ON, proceed with full boot ---

  // Initialize RoboEyes
  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, MAX_FPS);
  eyes.setAutoblinker(true, 3, 2);  // Auto blink every 3-5 seconds
  eyes.setIdleMode(true, 2, 3);     // Idle movements every 2-5 seconds

  // Configure eye appearance
  eyes.setWidth(45, 45);
  eyes.setHeight(30, 30);
  eyes.setBorderradius(10, 10);
  eyes.setSpacebetween(15);

  showStartupMessage(); // Show startup message
  connectToWiFi(); // Connect to WiFi
  wakeUpEyes(); // Wake up eyes
  dht.begin(); // Initialize DHT sensor
  pinMode(TOUCH_PIN, INPUT); // Setup touch sensor

  // Setup ultrasonic sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

// ============ MAIN LOOP ============
void loop() {
  checkPresence();
  if (digitalRead(POWER_SWITCH_PIN) == LOW) goToSleep();
  handleTouch();
  updateState();
  
  // Only update eyes when not showing info displays
  if (currentState != SHOWING_TIME && currentState != STOPWATCH_MODE && currentState != SHOWING_DISTANCE && currentState != SHOWING_TEMPERATURE && currentState != SHOWING_HUMIDITY && currentState != POMODORO_SELECT && currentState != POMODORO_RUNNING && currentState != BREAK_REMINDER && currentState != SPOTIFY_MODE && currentState != POSTURE_ALERT && currentState != WATER_REMINDER && currentState != BRIGHTNESS_ADJUST && currentState != DISTANCE_ADJUST) {
    eyes.update();
  }
}

// ============ STARTUP MESSAGE ============
void showStartupMessage() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  display.setTextSize(2);
  display.setCursor(2, 15);
  display.print("Chuha Pie");
  display.setTextSize(1);
  display.setCursor(114, 23);
  display.print("V1");
  
  display.setTextSize(1);
  display.setCursor(2, 40);
  display.println("Built for:");
  display.setCursor(30, 50);
  display.println("Yashi Pie <3");
  
  display.display();
  delay(3000);
  
  display.clearDisplay();
  display.display();
}

// ============ WAKE UP ANIMATION ============
void wakeUpEyes() {
  // Force eyes to center position before starting
  eyes.setPosition(DEFAULT);
  eyes.setMood(DEFAULT);
  eyes.setIdleMode(false, 2, 3);  // Disable idle during wake-up
  
  // Wake up sequence
  eyes.close();
  for(int i = 0; i < 30; i++) { eyes.update(); delay(33); }  // ~1s closed
  
  // Blink a few times (waking up)
  for(int i = 0; i < 3; i++) {
    eyes.open();
    for(int j = 0; j < 6; j++) { eyes.update(); delay(33); }  // ~200ms open
    eyes.close();
    for(int j = 0; j < 6; j++) { eyes.update(); delay(33); }  // ~200ms closed
  }
  
  // Open with happy mood
  eyes.open();
  eyes.setMood(HAPPY);
  for(int i = 0; i < 20; i++) { eyes.update(); delay(50); }  // ~1s happy
  
  // Settle to default
  eyes.setMood(DEFAULT);
  eyes.setPosition(DEFAULT);
  for(int i = 0; i < 30; i++) { eyes.update(); delay(33); }  // ~1s settle
  
  // Re-enable idle mode for main loop
  eyes.setIdleMode(true, 2, 3);
}

// ============ DISTANCE MEASUREMENT ============
// Single raw reading from HC-SR04
float getDistanceRaw() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  float distance = duration * 0.034 / 2;
  return distance;
}

// Smoothed reading: median of 5 rapid samples -> EMA filter
// Median rejects outlier spikes, EMA smooths valid jitter
float getDistance() {
  // Take 5 rapid readings
  float samples[5];
  for (int i = 0; i < 5; i++) {
    samples[i] = getDistanceRaw();
    if (i < 4) delayMicroseconds(500);  // Brief pause between pings
  }
  
  // Sort for median (simple insertion sort on 5 elements)
  for (int i = 1; i < 5; i++) {
    float key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }
  float median = samples[2];  // Middle value
  
  // Apply EMA (exponential moving average)
  if (!smoothedDistanceInit || median <= 0 || median >= 400) {
    // Seed EMA on first valid reading, or pass through invalid
    if (median > 0 && median < 400) {
      smoothedDistance = median;
      smoothedDistanceInit = true;
    }
    return median;
  }
  
  smoothedDistance = EMA_ALPHA * median + (1.0 - EMA_ALPHA) * smoothedDistance;
  return smoothedDistance;
}

// ============ PRESENCE DETECTION ============
void checkPresence() {
  // Only check at intervals to avoid constant polling
  if (millis() - lastDistanceCheck < DISTANCE_CHECK_INTERVAL) {
    return;
  }
  lastDistanceCheck = millis();
  
  float distance = getDistance();
  wasPersonPresent = personPresent;
  
  // Check smoothed distance against threshold (debounce logic below)
  bool currentlyDetected = (distance > 0 && distance < detectionOptions[distanceSelected]);
  
  // Track when raw detection state changed
  if (currentlyDetected != rawPresenceDetected) {
    rawPresenceDetected = currentlyDetected;
    presenceChangeTime = millis();
  }
  
  // Debounced presence: require sustained detection/absence
  if (rawPresenceDetected && !personPresent) {
    // Someone detected but not yet confirmed
    if (millis() - presenceChangeTime >= PRESENCE_ARRIVE_DELAY) {
      personPresent = true;
    }
  } else if (!rawPresenceDetected && personPresent) {
    // No one detected but still marked present — wait 5 seconds
    if (millis() - presenceChangeTime >= PRESENCE_GONE_DELAY) {
      personPresent = false;
    }
  }
  
  // Check for state transitions
  if (personPresent && !wasPersonPresent) {
    // Person just arrived - start greeting sequence
    greetingState = GREETING_CURIOUS;
    greetingStartTime = millis();
    continuousPresenceStart = millis();  // Start tracking for break reminder
    breakReminderShown = false;
  } else if (!personPresent && wasPersonPresent) {
    // Person left - reset greeting and break timer
    greetingState = NO_GREETING;
    breakReminderShown = false;
    lastPresenceEnd = millis();  // Track when they left for water reminder
    waterReturnReminderPending = false;
  }
  
  // --- Posture alert: too close detection (any mode) ---
  bool currentlyTooClose = (distance > 0 && distance < postureOptions[distanceSelected]);
  if (currentlyTooClose && !tooCloseDetected) {
    tooCloseDetected = true;
    tooCloseStartTime = millis();
  } else if (!currentlyTooClose) {
    tooCloseDetected = false;
    postureAlertActive = false;  // Reset so next episode can trigger
  }
  
  // Fire posture alert if sustained too close for 3s (with cooldown)
  if (tooCloseDetected && !postureAlertActive &&
      (millis() - tooCloseStartTime >= POSTURE_SUSTAIN_TIME) &&
      (millis() - lastPostureAlert >= POSTURE_COOLDOWN) &&
      currentState != POSTURE_ALERT && currentState != BREAK_REMINDER && currentState != WATER_REMINDER &&
      currentState != WAITING_FOR_SECOND_TAP && currentState != WAITING_FOR_THIRD_TAP) {
    postureAlertActive = true;
    lastPostureAlert = millis();
    previousState = currentState;
    currentState = POSTURE_ALERT;
    stateStartTime = millis();
  }
  
  // --- Break reminder: 1 hour continuous presence (any mode) ---
  if (personPresent && !breakReminderShown &&
      (millis() - continuousPresenceStart >= BREAK_REMINDER_INTERVAL) &&
      currentState != BREAK_REMINDER && currentState != POSTURE_ALERT &&
      currentState != WAITING_FOR_SECOND_TAP && currentState != WAITING_FOR_THIRD_TAP) {
    breakReminderShown = true;
    previousState = currentState;
    currentState = BREAK_REMINDER;
    stateStartTime = millis();
  }
  
  // --- Water reminder: hourly + 1min after return from long absence ---
  bool canShowWater = (currentState != WATER_REMINDER && currentState != BREAK_REMINDER && currentState != POSTURE_ALERT &&
                       currentState != WAITING_FOR_SECOND_TAP && currentState != WAITING_FOR_THIRD_TAP);
  
  // Trigger 1: Person returned after long absence (>30min) — remind after 1min
  if (personPresent && !wasPersonPresent && lastPresenceEnd > 0) {
    unsigned long absenceDuration = millis() - lastPresenceEnd;
    if (absenceDuration >= LONG_ABSENCE_THRESHOLD) {
      waterReturnReminderPending = true;
      waterReturnDetectTime = millis();
    }
  }
  
  if (waterReturnReminderPending && personPresent && canShowWater &&
      (millis() - waterReturnDetectTime >= WATER_RETURN_DELAY)) {
    waterReturnReminderPending = false;
    lastWaterReminder = millis();
    previousState = currentState;
    currentState = WATER_REMINDER;
    stateStartTime = millis();
  }
  
  // Trigger 2: Hourly reminder while present
  if (personPresent && canShowWater && lastWaterReminder > 0 &&
      (millis() - lastWaterReminder >= WATER_REMINDER_INTERVAL)) {
    lastWaterReminder = millis();
    previousState = currentState;
    currentState = WATER_REMINDER;
    stateStartTime = millis();
  }
  
  // Seed the first water reminder timestamp on first presence
  if (personPresent && lastWaterReminder == 0) {
    lastWaterReminder = millis();
  }
  
  // Update greeting sequence
  if (greetingState != NO_GREETING && greetingState != GREETING_DONE) {
    unsigned long greetingTime = millis() - greetingStartTime;
    
    if (greetingState == GREETING_CURIOUS && greetingTime > 1000) {
      greetingState = GREETING_HAPPY;
      greetingStartTime = millis();
    } else if (greetingState == GREETING_HAPPY && greetingTime > 3000) {
      greetingState = GREETING_DONE;
    }
  }
}

// ============ WIFI CONNECTION ============
void connectToWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Line 1: WiFi status
  display.setCursor(0, 5);
  display.print("WiFi: Connecting");
  display.display();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    // Animate 1-3 dots, cycling
    dotCount = (dotCount % 3) + 1;
    display.fillRect(96, 5, 32, 8, SSD1306_BLACK);  // Clear dot area
    display.setCursor(96, 5);
    for (int i = 0; i < dotCount; i++) display.print(".");
    display.display();
    attempts++;
  }
  
  // Line 2: WiFi result
  display.setCursor(0, 18);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("> Connected!");
    display.display();
    delay(500);
    
    // Line 3: Time sync status
    display.setCursor(0, 31);
    display.print("Time: Syncing...");
    display.display();
    
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    delay(2000);
    
    // Line 4: Time sync result
    struct tm timeinfo;
    display.setCursor(0, 44);
    if (getLocalTime(&timeinfo, 5000)) {
      display.print("> Synced!");
      display.display();
      
      // Set brightness based on time of day
      int hour = timeinfo.tm_hour;
      if (hour >= 7 && hour < 19) {
        brightnessSelected = 4;  // 7AM-7PM: 100%
      } else {
        brightnessSelected = 2;  // 7PM-7AM: 50%
      }
    } else {
      display.print("> Sync failed!");
      display.display();
      brightnessSelected = 3;  // Fallback: 75%
    }
    applyBrightness();
  } else {
    display.print("> Failed!");
    display.display();
    
    // No WiFi: default to 75% brightness
    brightnessSelected = 3;
    applyBrightness();
  }
  
  delay(2000);
  display.clearDisplay();
  display.display();
}

// ============ TIME DISPLAY ============
void displayCurrentTime() {
  // Check WiFi first to avoid blocking getLocalTime call (default 5s timeout)
  if (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 22);
    display.println("Time & Date N/A");
    display.setCursor(7, 34);
    display.println("Wifi not connected!");
    display.display();
    return;
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {  // Short 100ms timeout to avoid blocking loop
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(1, 22);
    display.println("Time/Date Unavailable");
    display.setCursor(16, 34);
    display.println("NTP sync failed!");
    display.display();
    return;
  }
  
  // Format time based on 12hr/24hr toggle
  char timeStr[12];
  if (time24HourFormat) {
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  } else {
    strftime(timeStr, sizeof(timeStr), "%I:%M:%S", &timeinfo);
  }
  
  // AM/PM indicator for 12hr mode
  char ampm[3] = "";
  if (!time24HourFormat) {
    strftime(ampm, sizeof(ampm), "%p", &timeinfo);
  }
  
  // Format date as DD-MM-YYYY
  char dateStr[15];
  strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", &timeinfo);
  
  // Display time
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Large time display
  display.setTextSize(2);
  if (time24HourFormat) {
    display.setCursor(17, 15);
    display.println(timeStr);
  } else {
    display.setCursor(10, 15);
    display.print(timeStr);
    display.setTextSize(1);
    display.print(ampm);
  }
  
  // Date below
  display.setTextSize(1);
  display.setCursor(35, 40);
  display.println(dateStr);
  
  display.display();
}

// ============ STOPWATCH DISPLAY ============
void displayStopwatch() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Header - "Stopwatch"
  display.setTextSize(1);
  display.setCursor(38, 2);
  display.println("STOPWATCH");
  display.drawLine(0, 11, 128, 11, SSD1306_WHITE);
  
  // Calculate current elapsed time
  unsigned long totalMs = stopwatchElapsed;
  if (stopwatchRunning) {
    totalMs += (millis() - stopwatchStartTime);
  }
  
  unsigned long totalSec = totalMs / 1000;
  unsigned long ms = (totalMs % 1000) / 10;  // Centiseconds
  unsigned long sec = totalSec % 60;
  unsigned long mins = (totalSec / 60) % 60;
  unsigned long hrs = totalSec / 3600;
  
  // Large time display (size 3) with centiseconds (size 1) on same line
  char timeStr[16];
  if (hrs > 0) {
    sprintf(timeStr, "%lu:%02lu:%02lu", hrs, mins, sec);
  } else {
    sprintf(timeStr, "%02lu:%02lu", mins, sec);
  }
  
  char csStr[6];
  sprintf(csStr, ".%02lu", ms);
  
  // Calculate positions: textSize 3 = 18px/char, textSize 1 = 6px/char
  int mainWidth = strlen(timeStr) * 18;
  int csWidth = strlen(csStr) * 6;
  int totalWidth = mainWidth + csWidth;
  int startX = (128 - totalWidth) / 2;
  int mainY = 24;  // Vertically centered below header
  
  display.setTextSize(3);
  display.setCursor(startX, mainY);
  display.print(timeStr);
  
  // Centiseconds at size 1, aligned to bottom of the large text
  // textSize 3 = 24px tall, textSize 1 = 8px tall, so offset by 16px
  display.setTextSize(1);
  display.setCursor(startX + mainWidth, mainY + 16);
  display.print(csStr);
  
  // Show "Paused" at bottom when paused
  if (!stopwatchRunning && stopwatchElapsed > 0) {
    display.setTextSize(1);
    display.setCursor(46, 54);
    display.println("Paused");
  }
  
  display.display();
}

void displayDistance() {
  // Only update display every 200ms to prevent flickering
  unsigned long currentTime = millis();
  if (currentTime - lastDistanceDisplayUpdate < 200) {
    return;
  }
  lastDistanceDisplayUpdate = currentTime;
  
  float distance = getDistance();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.println("DISTANCE:");
  
  // Large distance display
  display.setTextSize(3);
  if (distance > 0 && distance < 400) {
    // Valid reading
    display.setCursor(30, 30);
    if (distance < 100) {
      display.print(distance, 1);  // Show 1 decimal for < 100cm
    } else {
      display.print((int)distance);  // No decimals for larger values
    }
    display.setTextSize(2);
    display.println("cm");
  } else {
    // Out of range
    display.setCursor(25, 30);
    display.println("---");
  }
  
  display.display();
}

void displayTemperature() {
  float temperature = dht.readTemperature();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Check if reading is valid
  if (isnan(temperature)) {
    display.setTextSize(1);
    display.setCursor(15, 25);
    display.println("Sensor Error!");
    display.display();
    return;
  }
  
  // Title
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.println("TEMPERATURE:");
  
  // Large temperature display
  display.setTextSize(3);
  display.setCursor(25, 30);
  display.print(temperature, 1);
  display.setTextSize(2);
  display.println("C");
  
  display.display();
}

void displayHumidity() {
  float humidity = dht.readHumidity();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Check if reading is valid
  if (isnan(humidity)) {
    display.setTextSize(1);
    display.setCursor(15, 25);
    display.println("Sensor Error!");
    display.display();
    return;
  }
  
  // Title
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.println("HUMIDITY:");
  
  // Large humidity display
  display.setTextSize(3);
  display.setCursor(45, 30);
  display.print(humidity, 0);
  display.setTextSize(2);
  display.println("%");
  
  display.display();
}

// ============ BRIGHTNESS DISPLAY ============
void displayBrightness() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Header
  display.setTextSize(1);
  display.setCursor(25, 2);
  display.print("BRIGHTNESS (%)");
  display.drawLine(4, 11, 123, 11, SSD1306_WHITE);
  
  // Options in horizontal row: 5, 25, 50, 75, 100
  // Each option area: ~24px wide, 5 options = 120px, starting at x=4
  int optionWidth = 24;
  int startX = 4;
  int optionY = 24;
  
  display.setTextSize(1);
  for (int i = 0; i < 5; i++) {
    int x = startX + (i * optionWidth);
    
    if (i == brightnessSelected) {
      // Draw thin border around selected option
      display.drawRect(x, optionY - 2, optionWidth, 14, SSD1306_WHITE);
    }
    
    // Center the number text within each option area
    char label[5];
    sprintf(label, "%d", brightnessOptions[i]);
    int labelWidth = strlen(label) * 6;  // 6px per char at textSize 1
    int textX = x + (optionWidth - labelWidth) / 2;
    display.setCursor(textX, optionY + 1);
    display.print(label);
  }
  
  // Progress bar at bottom (matching Spotify style)
  int barWidth = map(brightnessOptions[brightnessSelected], 0, 100, 0, 116);
  display.drawRect(4, 55, 120, 7, SSD1306_WHITE);
  if (barWidth > 0) display.fillRect(6, 57, barWidth, 3, SSD1306_WHITE);
  
  display.display();
}

// ============ DISTANCE SETTINGS DISPLAY ============
void displayDistanceAdjust() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(8, 2);
  display.print("PRESENCE RANGE (cm)");
  display.drawLine(4, 11, 123, 11, SSD1306_WHITE);

  // Options in horizontal row: 30, 50, 75, 100, 125 cm
  int optionWidth = 24;
  int startX = 4;
  int optionY = 24;

  display.setTextSize(1);
  for (int i = 0; i < 5; i++) {
    int x = startX + (i * optionWidth);

    if (i == distanceSelected) {
      display.drawRect(x, optionY - 2, optionWidth, 14, SSD1306_WHITE);
    }

    char label[5];
    sprintf(label, "%d", detectionOptions[i]);
    int labelWidth = strlen(label) * 6;
    int textX = x + (optionWidth - labelWidth) / 2;
    display.setCursor(textX, optionY + 1);
    display.print(label);
  }

  // Posture threshold hint
  display.setCursor(4, 44);
  display.print("Posture alert: <");
  display.print(postureOptions[distanceSelected]);
  display.print("cm");

  // Progress bar at bottom
  int barWidth = map(distanceSelected, 0, 4, 0, 116);
  display.drawRect(4, 55, 120, 7, SSD1306_WHITE);
  if (barWidth > 0) display.fillRect(6, 57, barWidth, 3, SSD1306_WHITE);

  display.display();
}

// ============ POMODORO DISPLAY ============
void displayPomodoroSelect() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setTextSize(1);
  display.setCursor(10, 2);
  display.println("POMODORO:");
  
  // Show all 5 options, highlight selected
  for (int i = 0; i < 5; i++) {
    int y = 14 + i * 10;
    display.setCursor(20, y);
    
    if (i == pomodoroSelected) {
      // Highlight: draw filled rect behind selected option
      display.fillRect(16, y - 1, 96, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.print("> ");
      display.println(pomodoroLabels[i]);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.print("  ");
      display.println(pomodoroLabels[i]);
    }
  }
  
  // Show countdown to auto-start
  unsigned long elapsed = millis() - pomodoroSelectionTime;
  int remaining = 10 - (elapsed / 1000);
  if (remaining < 0) remaining = 0;
  
  display.setCursor(100, 2);
  display.print(remaining);
  display.print("s");
  
  display.display();
}

void displayPomodoroCountdown() {
  unsigned long remaining = 0;
  if (pomodoroPaused) {
    remaining = pomodoroPausedRemaining;
  } else if (millis() < pomodoroEndTime) {
    remaining = pomodoroEndTime - millis();
  }
  
  unsigned long totalSecs = remaining / 1000;
  int hours = totalSecs / 3600;
  int mins = (totalSecs % 3600) / 60;
  int secs = totalSecs % 60;
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Title - show "Paused" instead of time mode label when paused
  display.setTextSize(1);
  display.setCursor(10, 5);
  display.print("Pomodoro: ");
  if (pomodoroPaused) {
    display.print("Paused");
  } else {
    display.print(pomodoroLabels[pomodoroSelected]);
  }
  
  // Large countdown
  char timeStr[10];
  if (hours > 0) {
    sprintf(timeStr, "%d:%02d:%02d", hours, mins, secs);
    display.setTextSize(2);
    display.setCursor(15, 25);
  } else {
    sprintf(timeStr, "%02d:%02d", mins, secs);
    display.setTextSize(3);
    display.setCursor(25, 22);
  }
  display.println(timeStr);
  
  // Progress bar
  unsigned long totalTime = (unsigned long)pomodoroOptions[pomodoroSelected] * 60000UL;
  unsigned long elapsed = totalTime - remaining;
  int barWidth = map(elapsed, 0, totalTime, 0, 120);
  if (barWidth > 120) barWidth = 120;
  display.drawRect(4, 55, 120, 7, SSD1306_WHITE);
  if (barWidth > 0) display.fillRect(6, 57, barWidth, 3, SSD1306_WHITE);
  
  display.display();
}

// ============ SPOTIFY API ============
bool spotifyRefreshAccessToken() {
  Serial.println("[Spotify] Refreshing access token...");
  WiFiClientSecure client;
  client.setInsecure();  // Skip cert verification (saves RAM)
  
  HTTPClient http;
  http.begin(client, "https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Build Basic auth header using ESP32 built-in Base64
  String credentials = String(SPOTIFY_CLIENT_ID) + ":" + String(SPOTIFY_CLIENT_SECRET);
  String encoded = base64::encode(credentials);
  
  http.addHeader("Authorization", "Basic " + encoded);
  
  String body = "grant_type=refresh_token&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN);
  int httpCode = http.POST(body);
  
  Serial.print("[Spotify] Token response code: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    String response = http.getString();
    // Parse access_token from JSON (simple extraction)
    int tokenStart = response.indexOf("\"access_token\":\"") + 16;
    int tokenEnd = response.indexOf("\"", tokenStart);
    if (tokenStart > 15 && tokenEnd > tokenStart) {
      spotifyAccessToken = response.substring(tokenStart, tokenEnd);
      spotifyTokenExpiry = millis() + 3500000UL;  // ~58 minutes
      Serial.println("[Spotify] Token refreshed OK");
      http.end();
      return true;
    }
  } else {
    Serial.print("[Spotify] Token refresh FAILED: ");
    Serial.println(http.getString());
  }
  http.end();
  return false;
}

bool spotifyApiCall(const char* method, const char* endpoint, String* responseOut = nullptr) {
  // Auto-refresh token if expired
  if (millis() > spotifyTokenExpiry || spotifyAccessToken.length() == 0) {
    if (!spotifyRefreshAccessToken()) return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String url = "https://api.spotify.com/v1/me/player" + String(endpoint);
  http.begin(client, url);
  http.addHeader("Authorization", "Bearer " + spotifyAccessToken);
  http.addHeader("Content-Length", "0");
  
  int httpCode;
  if (strcmp(method, "PUT") == 0) {
    httpCode = http.PUT("");
  } else if (strcmp(method, "POST") == 0) {
    httpCode = http.POST("");
  } else {
    httpCode = http.GET();
  }
  
  Serial.print("[Spotify] ");
  Serial.print(method);
  Serial.print(" ");
  Serial.print(endpoint);
  Serial.print(" -> ");
  Serial.println(httpCode);
  
  bool success = (httpCode >= 200 && httpCode < 300);
  if (success && responseOut && httpCode != 204) {
    *responseOut = http.getString();
  }
  if (httpCode == 204 && responseOut) {
    *responseOut = "";  // No content
  }
  if (!success) {
    Serial.print("[Spotify] Error body: ");
    Serial.println(http.getString());
  }
  http.end();
  return success;
}

void spotifyPlayPause() {
  if (spotifyIsPlaying) {
    // Snapshot interpolated progress before pausing so display doesn't jump back
    if (spotifyLastProgressUpdate > 0) {
      spotifyProgressMs += (millis() - spotifyLastProgressUpdate);
      if (spotifyProgressMs > spotifyDurationMs) spotifyProgressMs = spotifyDurationMs;
      spotifyLastProgressUpdate = millis();
    }
    spotifyApiCall("PUT", "/pause");
  } else {
    spotifyApiCall("PUT", "/play");
    spotifyLastProgressUpdate = millis();  // Resume interpolation from current value
  }
  spotifyIsPlaying = !spotifyIsPlaying;
}

void spotifyNextTrack() {
  spotifyApiCall("POST", "/next");
  delay(300);  // Brief delay for Spotify to update
  spotifyGetCurrentTrack();  // Refresh display
}

void spotifyPrevTrack() {
  spotifyApiCall("POST", "/previous");
  delay(300);
  spotifyGetCurrentTrack();
}

// Unescape JSON string: handles \" \/ \\ \n \u00XX etc.
String jsonUnescape(String s) {
  String result = "";
  result.reserve(s.length());
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == '\\' && i + 1 < (int)s.length()) {
      char next = s[i + 1];
      if (next == '"') { result += '"'; i++; }
      else if (next == '\\') { result += '\\'; i++; }
      else if (next == '/') { result += '/'; i++; }
      else if (next == 'n') { result += ' '; i++; }  // newline -> space
      else if (next == 'r') { i++; }  // skip \r
      else if (next == 't') { result += ' '; i++; }  // tab -> space
      else if (next == 'u' && i + 5 < (int)s.length()) {
        // \uXXXX - parse hex, keep ASCII printable, replace others with ?
        String hex = s.substring(i + 2, i + 6);
        long code = strtol(hex.c_str(), NULL, 16);
        if (code >= 32 && code < 127) {
          result += (char)code;
        } else {
          result += '?';  // Non-ASCII can't display on SSD1306
        }
        i += 5;
      }
      else { result += s[i]; }  // Unknown escape, keep as-is
    } else {
      result += s[i];
    }
  }
  return result;
}

// Find the end of a JSON string value, handling escaped quotes
int findJsonStringEnd(const String& s, int startQuote) {
  int i = startQuote + 1;
  while (i < (int)s.length()) {
    if (s[i] == '\\') {
      i += 2;  // Skip escaped character
    } else if (s[i] == '"') {
      return i;
    } else {
      i++;
    }
  }
  return -1;
}

void spotifyGetCurrentTrack() {
  String response;
  if (spotifyApiCall("GET", "/currently-playing", &response)) {
    Serial.print("[Spotify] Response length: ");
    Serial.println(response.length());
    
    if (response.length() < 10) {
      spotifyTrackName = "No track";
      spotifyArtistName = "";
      spotifyIsPlaying = false;
      return;
    }
    
    // Helper: find "key" then skip whitespace/colon to get the quoted value
    // Spotify returns pretty-printed JSON: "key" : "value"
    
    // --- Parse is_playing first (top-level) ---
    int playIdx = response.indexOf("\"is_playing\"");
    if (playIdx >= 0) {
      // Find colon after key, then look for true/false
      int afterColon = response.indexOf(":", playIdx + 12);
      if (afterColon > 0) {
        String playVal = response.substring(afterColon + 1, afterColon + 10);
        playVal.trim();
        spotifyIsPlaying = playVal.startsWith("true");
      }
    }
    Serial.print("[Spotify] isPlaying: ");
    Serial.println(spotifyIsPlaying);
    
    // --- Find "item" object ---
    int itemIdx = response.indexOf("\"item\"");
    if (itemIdx < 0) {
      Serial.println("[Spotify] No item found in response");
      return;
    }
    
    // --- Parse track name: find first "name" after "item" that's at track level ---
    // Skip album section - find the track-level "name" by looking past "available_markets"
    // or by finding "disc_number" which appears in the track object
    int discIdx = response.indexOf("\"disc_number\"", itemIdx);
    int trackNameIdx = -1;
    if (discIdx > 0) {
      // Track "name" comes after disc_number/duration_ms/etc
      trackNameIdx = response.indexOf("\"name\"", discIdx);
    }
    if (trackNameIdx < 0) {
      // Fallback: just find first "name" after item + some offset to skip album names
      trackNameIdx = response.indexOf("\"name\"", itemIdx + 500);
      if (trackNameIdx < 0) trackNameIdx = response.indexOf("\"name\"", itemIdx);
    }
    
    if (trackNameIdx > 0) {
      int colonPos = response.indexOf(":", trackNameIdx + 5);
      if (colonPos > 0) {
        int quoteStart = response.indexOf("\"", colonPos + 1);
        if (quoteStart > 0) {
          int quoteEnd = findJsonStringEnd(response, quoteStart);
          if (quoteEnd > quoteStart + 1) {
            spotifyTrackName = jsonUnescape(response.substring(quoteStart + 1, quoteEnd));
            Serial.print("[Spotify] Track: ");
            Serial.println(spotifyTrackName);
            if (spotifyTrackName.length() > 20) {
              spotifyTrackName = spotifyTrackName.substring(0, 18) + "..";
            }
          }
        }
      }
    }
    
    // --- Parse artist name: find "artists" array after item, get first "name" ---
    // We want the track-level artists, not album artists
    int trackArtistIdx = -1;
    if (discIdx > 0) {
      trackArtistIdx = response.indexOf("\"artists\"", discIdx);
    }
    if (trackArtistIdx < 0) {
      // Fallback to first artists after item
      trackArtistIdx = response.indexOf("\"artists\"", itemIdx);
    }
    
    if (trackArtistIdx > 0) {
      int artNameIdx = response.indexOf("\"name\"", trackArtistIdx + 9);
      if (artNameIdx > 0) {
        int artColon = response.indexOf(":", artNameIdx + 5);
        if (artColon > 0) {
          int artQuoteStart = response.indexOf("\"", artColon + 1);
          if (artQuoteStart > 0) {
            int artQuoteEnd = findJsonStringEnd(response, artQuoteStart);
            if (artQuoteEnd > artQuoteStart + 1) {
              spotifyArtistName = jsonUnescape(response.substring(artQuoteStart + 1, artQuoteEnd));
              Serial.print("[Spotify] Artist: ");
              Serial.println(spotifyArtistName);
              if (spotifyArtistName.length() > 20) {
                spotifyArtistName = spotifyArtistName.substring(0, 18) + "..";
              }
            }
          }
        }
      }
    }
    // --- Parse progress_ms ---
    int progIdx = response.indexOf("\"progress_ms\"");
    if (progIdx >= 0) {
      int progColon = response.indexOf(":", progIdx + 13);
      if (progColon > 0) {
        int progStart = progColon + 1;
        // Skip whitespace
        while (progStart < (int)response.length() && response[progStart] == ' ') progStart++;
        int progEnd = progStart;
        while (progEnd < (int)response.length() && response[progEnd] >= '0' && response[progEnd] <= '9') progEnd++;
        if (progEnd > progStart) {
          spotifyProgressMs = response.substring(progStart, progEnd).toInt();
          spotifyLastProgressUpdate = millis();
        }
      }
    }
    
    // --- Parse duration_ms (inside item) ---
    if (discIdx > 0) {
      int durIdx = response.indexOf("\"duration_ms\"", discIdx);
      if (durIdx > 0) {
        int durColon = response.indexOf(":", durIdx + 13);
        if (durColon > 0) {
          int durStart = durColon + 1;
          while (durStart < (int)response.length() && response[durStart] == ' ') durStart++;
          int durEnd = durStart;
          while (durEnd < (int)response.length() && response[durEnd] >= '0' && response[durEnd] <= '9') durEnd++;
          if (durEnd > durStart) {
            spotifyDurationMs = response.substring(durStart, durEnd).toInt();
          }
        }
      }
    }
  } else {
    Serial.println("[Spotify] GetCurrentTrack API call failed");
  }
}

// ============ SPOTIFY DISPLAY ============
void displaySpotify() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Show N/A screen if WiFi not connected
  if (WiFi.status() != WL_CONNECTED) {
    display.setTextSize(1);
    display.setCursor(4, 22);
    display.println("Spotify control N/A");
    display.setCursor(7, 34);
    display.println("Wifi not connected!");
    display.display();
    return;
  }
  
  // Header
  display.setTextSize(1);
  display.setCursor(44, 2);
  display.println("SPOTIFY");
  display.drawLine(4, 11, 123, 11, SSD1306_WHITE);
  
  // Track name
  display.setTextSize(1);
  display.setCursor(4, 16);
  if (spotifyTrackName.length() > 0) {
    display.print(spotifyTrackName);
  } else {
    display.print("Loading...");
  }
  
  // Artist name
  display.setCursor(4, 28);
  display.setTextColor(SSD1306_WHITE);
  display.print(spotifyArtistName);
  
  // Play/Pause icon (small, left side)
  display.drawLine(4, 39, 123, 39, SSD1306_WHITE);
  if (spotifyIsPlaying) {
    // Pause icon (two small bars)
    display.fillRect(4, 42, 3, 8, SSD1306_WHITE);
    display.fillRect(10, 42, 3, 8, SSD1306_WHITE);
  } else {
    // Play icon (small triangle)
    for (int i = 0; i < 8; i++) {
      display.drawLine(4, 42 + i, 4 + (i < 4 ? i : 7 - i), 42 + i, SSD1306_WHITE);
    }
  }
  
  // Elapsed time next to icon
  unsigned long currentProgress = spotifyProgressMs;
  if (spotifyIsPlaying && spotifyLastProgressUpdate > 0) {
    currentProgress += (millis() - spotifyLastProgressUpdate);
    if (currentProgress > spotifyDurationMs) currentProgress = spotifyDurationMs;
  }
  int elapsedSec = currentProgress / 1000;
  int elapsedMin = elapsedSec / 60;
  elapsedSec %= 60;
  int totalSec = spotifyDurationMs / 1000;
  int totalMin = totalSec / 60;
  totalSec %= 60;
  
  char timeStr[14];
  sprintf(timeStr, "%d:%02d / %d:%02d", elapsedMin, elapsedSec, totalMin, totalSec);
  int timeWidth = strlen(timeStr) * 6;  // 6px per char at textSize 1
  display.setTextSize(1);
  display.setCursor(128 - timeWidth - 2, 44);
  display.print(timeStr);
  
  // Progress bar at bottom
  display.drawRect(4, 55, 120, 7, SSD1306_WHITE);
  if (spotifyDurationMs > 0) {
    int barWidth = (int)((currentProgress * 116UL) / spotifyDurationMs);
    if (barWidth > 116) barWidth = 116;
    if (barWidth > 0) display.fillRect(6, 57, barWidth, 3, SSD1306_WHITE);
  }
  
  display.display();
}

// ============ HAPTIC FEEDBACK ============
void vibrate(int duration) {
  digitalWrite(VIBRATION_PIN, HIGH);
  delay(duration);
  digitalWrite(VIBRATION_PIN, LOW);
}

void vibratePattern(int pulses, int pulseDuration, int gapDuration) {
  for (int i = 0; i < pulses; i++) {
    digitalWrite(VIBRATION_PIN, HIGH);
    delay(pulseDuration);
    digitalWrite(VIBRATION_PIN, LOW);
    if (i < pulses - 1) {
      delay(gapDuration);
    }
  }
}

// ============ RGB LED HELPERS ============
// Common anode: PWM 0 = full brightness, 255 = off
void ledSetColor(byte r, byte g, byte b) {
  ledcWrite(LED_RED_CH, 255 - r);
  ledcWrite(LED_GREEN_CH, 255 - g);
  ledcWrite(LED_BLUE_CH, 255 - b);
}

void ledOff() {
  ledcWrite(LED_RED_CH, 255);
  ledcWrite(LED_GREEN_CH, 255);
  ledcWrite(LED_BLUE_CH, 255);
}

// ============ BREAK REMINDER DISPLAY ============
void displayBreakReminder() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Draw border
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(1, 1, 126, 62, SSD1306_WHITE);
  
  // Main message centered
  display.setTextSize(1);
  display.setCursor(14, 18);
  display.println("Take a break,");
  display.setCursor(14, 30);
  display.println("Miss Sondhi :)");
  
  // Duration at bottom center
  display.setCursor(43, 50);
  display.println("1 Hour");
  
  display.display();
}

// ============ POSTURE ALERT DISPLAY ============
void displayPostureAlert() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Draw border
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(1, 1, 126, 62, SSD1306_WHITE);
  
  // Main message centered
  display.setTextSize(1);
  display.setCursor(16, 18);
  display.println("Too close! :(");
  display.setCursor(10, 30);
  display.println("Please sit back a");
  display.setCursor(7, 42);
  display.println("little Miss Sondhi!");
  
  display.display();
}

// ============ WATER REMINDER DISPLAY ============
void displayWaterReminder() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Draw border
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(1, 1, 126, 62, SSD1306_WHITE);
  
  // Main message centered
  display.setTextSize(1);
  display.setCursor(16, 18);
  display.println("Drink some water,");
  display.setCursor(14, 30);
  display.println("Miss Sondhi :)");
  display.setCursor(22, 46);
  display.println("Stay hydrated <3");
  
  display.display();
}

// ============ POWER SWITCH / DEEP SLEEP ============
void goToSleep() {
  // Clean shutdown before sleeping
  digitalWrite(VIBRATION_PIN, LOW);
  ledOff();
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  // Wake when switch is flipped back ON (GPIO35 goes HIGH)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, HIGH);
  esp_deep_sleep_start();
}

// ============ TOUCH HANDLING ============
void handleTouch() {
  bool currentTouch = digitalRead(TOUCH_PIN);
  
  // Rising edge - touch started
  if (currentTouch && !lastTouchState) {
    touchStartTime = millis();
    isLongPress = false;
    homePressTriggered = false;
  }
  
  // While touching
  if (currentTouch) {
    touchDuration = millis() - touchStartTime;
    
    // Universal home shortcut: 4s press returns to face from any display mode
    if (touchDuration >= 4000 && !homePressTriggered &&
        (currentState == SHOWING_TIME || currentState == STOPWATCH_MODE ||
         currentState == POMODORO_SELECT || currentState == POMODORO_RUNNING ||
         currentState == SPOTIFY_MODE || currentState == SHOWING_DISTANCE ||
         currentState == SHOWING_TEMPERATURE || currentState == SHOWING_HUMIDITY)) {
      homePressTriggered = true;
      isLongPress = true;
      vibratePattern(2, 100, 80);  // Double-pulse to distinguish from mode cycle
      // Clean up any mode-specific state
      spotifyTapCount = 0;
      spotifyTapPending = false;
      pomodoroPaused = false;
      stopwatchWaitingSecondTap = false;
      digitalWrite(VIBRATION_PIN, LOW);
      currentState = IDLE;
      eyes.setMood(DEFAULT);
      eyes.setPosition(DEFAULT);
      stateStartTime = millis();
    }
    // Pomodoro select: long press exits to Spotify
    else if (currentState == POMODORO_SELECT) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = SPOTIFY_MODE;
        spotifyTapCount = 0;
        spotifyTapPending = false;
        // Clear stale data so display shows "Loading..." immediately
        spotifyTrackName = "";
        spotifyArtistName = "";
        spotifyProgressMs = 0;
        spotifyDurationMs = 0;
        spotifyLastProgressUpdate = 0;
        // Don't call spotifyGetCurrentTrack() here — it blocks ~1-2s
        // Let the normal poll cycle fetch data (fires within SPOTIFY_POLL_INTERVAL)
        lastSpotifyPoll = 0;  // Force immediate poll on next loop iteration
        stateStartTime = millis();
      }
    }
    // Showing time: long press goes to stopwatch
    else if (currentState == SHOWING_TIME) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = STOPWATCH_MODE;
        stopwatchWaitingSecondTap = false;
        stateStartTime = millis();
      }
    }
    // Stopwatch mode: long press goes to next mode (Pomodoro)
    else if (currentState == STOPWATCH_MODE) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = POMODORO_SELECT;
        pomodoroSelected = 0;
        pomodoroSelectionTime = millis();
        stateStartTime = millis();
      }
    }
    // Spotify mode: long press exits
    else if (currentState == SPOTIFY_MODE) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        spotifyTapCount = 0;
        spotifyTapPending = false;
        currentState = SHOWING_DISTANCE;
        lastDistanceDisplayUpdate = 0;  // Force immediate first render
        stateStartTime = millis();
      }
    }
    // Pomodoro running: long press goes back to selection screen
    else if (currentState == POMODORO_RUNNING) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        pomodoroPaused = false;
        currentState = POMODORO_SELECT;
        pomodoroSelected = 0;
        pomodoroSelectionTime = millis();
        stateStartTime = millis();
      }
    }
    // Distance: long press cycles to temperature
    else if (currentState == SHOWING_DISTANCE) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = SHOWING_TEMPERATURE;
        stateStartTime = millis();
      }
    }
    // Temperature: long press cycles to humidity
    else if (currentState == SHOWING_TEMPERATURE) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = SHOWING_HUMIDITY;
        stateStartTime = millis();
      }
    }
    // Humidity: long press cycles back to eyes
    else if (currentState == SHOWING_HUMIDITY) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = IDLE;
        eyes.setMood(DEFAULT);
        eyes.setPosition(DEFAULT);
        stateStartTime = millis();
      }
    }
    // Brightness adjust: long press opens Distance settings
    else if (currentState == BRIGHTNESS_ADJUST) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = DISTANCE_ADJUST;
        stateStartTime = millis();
      }
    }
    // Distance adjust: long press exits back to eyes
    else if (currentState == DISTANCE_ADJUST) {
      if (touchDuration >= 1000 && !isLongPress) {
        isLongPress = true;
        vibrate(200);
        currentState = IDLE;
        eyes.setMood(DEFAULT);
        eyes.setPosition(DEFAULT);
        stateStartTime = millis();
      }
    }
    // Normal touch handling for all other states
    else {
      // Short press building (0-1s)
      if (touchDuration < 1000) {
        if (currentState == IDLE) {
          // Just show normal eyes, wait for release or long press
        }
      }
      // Long press building (1-2s)
      else if (touchDuration < 2000) {
        if (!isLongPress && currentState != LONG_PRESS_BUILDING) {
          currentState = LONG_PRESS_BUILDING;
          stateStartTime = millis();
        }
      }
      // Very long press - show affection! (>2s)
      else if (touchDuration >= 2000) {
        if (!isLongPress) {
          isLongPress = true;
          // Normal long press - show affection
          vibrate(200);  // Initial long press haptic feedback
          currentState = SHOWING_AFFECTION;
          stateStartTime = millis();
        }
      }
    }
  }
  
  // Falling edge - touch released
  if (!currentTouch && lastTouchState) {
    touchDuration = millis() - touchStartTime;
    // Handle different touch types
    if (isLongPress) {
      // Long press was shown, do nothing (already showing affection)
      isLongPress = false;
    }
    else if (touchDuration < 300 || (touchDuration < 600 && currentState == SPOTIFY_MODE)) {
      // Quick tap detected (300ms normally, 600ms in Spotify mode for easier multi-tap)
      unsigned long timeSinceLastTap = millis() - lastTapTime;
      
      if (currentState == SPOTIFY_MODE) {
        // Spotify multi-tap: accumulate taps within window
        spotifyTapCount++;
        spotifyLastTapTime = millis();
        spotifyTapPending = true;
        // Non-blocking quick pulse feedback
        digitalWrite(VIBRATION_PIN, HIGH);
        delay(20);
        digitalWrite(VIBRATION_PIN, LOW);
      } else if (timeSinceLastTap < 500 && currentState == WAITING_FOR_SECOND_TAP) {
        // DOUBLE TAP - wait for potential triple
        currentState = WAITING_FOR_THIRD_TAP;
        stateStartTime = millis();
        lastTapTime = millis();
      } else if (timeSinceLastTap < 500 && currentState == WAITING_FOR_THIRD_TAP) {
        // TRIPLE TAP - open brightness adjust!
        vibrate(200);
        currentState = BRIGHTNESS_ADJUST;
        stateStartTime = millis();
        lastTapTime = 0;
      } else if (currentState == STOPWATCH_MODE) {
        // Stopwatch: single/double tap handling
        if (stopwatchWaitingSecondTap && (millis() - stopwatchLastDoubleTap < 500)) {
          // Double tap = reset
          stopwatchWaitingSecondTap = false;
          stopwatchRunning = false;
          stopwatchElapsed = 0;
          vibrate(200);
        } else {
          // First tap - mark as waiting for potential second tap
          stopwatchWaitingSecondTap = true;
          stopwatchLastDoubleTap = millis();
        }
        lastTapTime = 0;
      } else if (currentState == POMODORO_SELECT) {
        // Single tap = cycle through pomodoro options
        vibrate(150);
        pomodoroSelected = (pomodoroSelected + 1) % 5;
        pomodoroSelectionTime = millis();  // Reset 10s auto-start countdown
        lastTapTime = 0;
      } else if (currentState == POMODORO_RUNNING) {
        // Tap while pomodoro running - toggle pause/resume
        vibrate(200);
        if (pomodoroPaused) {
          // Resume: recalculate end time from saved remaining
          pomodoroEndTime = millis() + pomodoroPausedRemaining;
          pomodoroPaused = false;
        } else {
          // Pause: save remaining time
          pomodoroPausedRemaining = (millis() < pomodoroEndTime) ? (pomodoroEndTime - millis()) : 0;
          pomodoroPaused = true;
        }
        lastTapTime = 0;
      } else if (currentState == SHOWING_TIME) {
        // Tap in time mode: toggle 12hr/24hr format
        time24HourFormat = !time24HourFormat;
        vibrate(200);
        lastTapTime = 0;
      } else if (currentState == SHOWING_DISTANCE || currentState == SHOWING_TEMPERATURE || currentState == SHOWING_HUMIDITY) {
        // These modes use long press to cycle - ignore taps
        lastTapTime = 0;
      } else if (currentState == BRIGHTNESS_ADJUST) {
        // Tap cycles through brightness options
        vibrate(150);
        brightnessSelected = (brightnessSelected + 1) % 5;
        applyBrightness();
        lastTapTime = 0;
      } else if (currentState == DISTANCE_ADJUST) {
        // Tap cycles through distance options
        vibrate(150);
        distanceSelected = (distanceSelected + 1) % 5;
        lastTapTime = 0;
      } else {
        // First tap - wait for possible second tap
        currentState = WAITING_FOR_SECOND_TAP;
        stateStartTime = millis();
        lastTapTime = millis();
      }
    }
    
    touchDuration = 0;
  }
  
  lastTouchState = currentTouch;
}

// ============ STATE MACHINE ============
void updateState() {
  unsigned long stateTime = millis() - stateStartTime;
  
  switch(currentState) {
    case IDLE:
      // Periodic temperature check for sweat animation (both present and absent)
      if (millis() - lastTempCheck >= TEMP_CHECK_INTERVAL) {
        lastTempCheck = millis();
        float temp = dht.readTemperature();
        if (!isnan(temp)) {
          lastTemperature = temp;
        }
      }
      
      // Sweat logic: above threshold, sweat for 10s every 5 minutes
      if (lastTemperature > SWEAT_THRESHOLD) {
        if (!isSweating && (millis() - lastSweatStart >= SWEAT_INTERVAL)) {
          eyes.setSweat(true);
          isSweating = true;
          lastSweatStart = millis();
        }
        if (isSweating && (millis() - lastSweatStart >= SWEAT_DURATION)) {
          eyes.setSweat(false);
          isSweating = false;
        }
      } else if (isSweating) {
        eyes.setSweat(false);
        isSweating = false;
      }
      
      // Handle presence-based behavior
      if (!personPresent) {
        // Nobody detected - show tired only after 30s of absence
        if (lastPresenceEnd > 0 && (millis() - lastPresenceEnd >= 30000)) {
          eyes.setMood(TIRED);
        } else {
          eyes.setMood(DEFAULT);
        }
      } else {
        
        // Someone present - check greeting sequence
        if (greetingState == GREETING_CURIOUS) {
          eyes.setMood(DEFAULT);
          eyes.setCuriosity(true);
          eyes.setPosition(N);  // Look up curiously
        } else if (greetingState == GREETING_HAPPY) {
          eyes.setMood(HAPPY);
          eyes.setCuriosity(false);
          eyes.setPosition(DEFAULT);
        } else {
          // Greeting done or no greeting - normal idle
          eyes.setMood(DEFAULT);
          eyes.setCuriosity(false);
          // RoboEyes idle movements handle the rest
        }
      }
      break;
      
    case LONG_PRESS_BUILDING:
      // Show happy for 1 second during buildup
      eyes.setMood(HAPPY);
      eyes.setSweat(false);
      isSweating = false;
      
      // Return to idle if touch released
      if (!digitalRead(TOUCH_PIN)) {
        currentState = IDLE;
        eyes.setMood(DEFAULT);
        eyes.setPosition(DEFAULT);
      }
      break;
      
    case SHOWING_AFFECTION: {
      // Show happy with laugh animation - continuous loop while touched
      eyes.setMood(HAPPY);
      
      // Repeat laugh animation every 2 seconds (3 laughs per cycle)
      unsigned long cycleTime = stateTime % 2000;  // Loop every 2 seconds
      if (cycleTime < 50 || (cycleTime > 600 && cycleTime < 650) || (cycleTime > 1200 && cycleTime < 1250)) {
        eyes.anim_laugh();
      }
      
      // Continuous rapid vibration pulses synced with laughs (repeating every 2 seconds)
      // 10 pulses: 100ms on, 100ms off (each cycle is 200ms)
      int currentPulse = cycleTime / 200;       // Which pulse cycle we're in
      int withinPulse = cycleTime % 200;        // Position within current cycle
      
      if (currentPulse < 10 && withinPulse < 100) {
        digitalWrite(VIBRATION_PIN, HIGH);
      } else {
        digitalWrite(VIBRATION_PIN, LOW);
      }
      
      // Only exit when touch is released
      if (!digitalRead(TOUCH_PIN)) {
        currentState = RECOVERING;
        stateStartTime = millis();
        digitalWrite(VIBRATION_PIN, LOW);  // Ensure vibration stops
      }
      break;
    }
      
    case WAITING_FOR_SECOND_TAP:
      // Wait for potential second tap
      if (stateTime > 500) {
        // Timeout - it was a single tap, show time
        vibrate(200);  // Single tap confirmed feedback - consistent 200ms
        currentState = SHOWING_TIME;
        stateStartTime = millis();
      }
      break;
      
    case WAITING_FOR_THIRD_TAP:
      // Wait for potential third tap (double tap detected)
      if (stateTime > 500) {
        // Timeout - it was a double tap, bonk!
        currentState = BONKED;
        stateStartTime = millis();
      }
      break;
      
    case SHOWING_TIME:
      // Display current time continuously
      displayCurrentTime();
      
      // Note: Tap to cycle to stopwatch is handled in handleTouch()
      break;
    
    case STOPWATCH_MODE:
      // Display stopwatch
      displayStopwatch();
      
      // Resolve single tap after double-tap window (500ms)
      if (stopwatchWaitingSecondTap && (millis() - stopwatchLastDoubleTap >= 500)) {
        stopwatchWaitingSecondTap = false;
        // Single tap = toggle start/pause
        if (stopwatchRunning) {
          // Pause: accumulate elapsed
          stopwatchElapsed += (millis() - stopwatchStartTime);
          stopwatchRunning = false;
        } else {
          // Start/Resume
          stopwatchStartTime = millis();
          stopwatchRunning = true;
        }
        vibrate(200);
      }
      break;
    
    case SPOTIFY_MODE:
      // Display Spotify screen
      displaySpotify();
      
      // Only poll/act when WiFi is connected (API calls block the loop)
      if (WiFi.status() == WL_CONNECTED) {
        // Only poll when no taps are pending (API calls block the loop ~1s)
        if (!spotifyTapPending && (millis() - lastSpotifyPoll >= SPOTIFY_POLL_INTERVAL)) {
          spotifyGetCurrentTrack();
          lastSpotifyPoll = millis();
        }
        
        // Resolve pending taps after the tap window expires
        if (spotifyTapPending && (millis() - spotifyLastTapTime >= SPOTIFY_TAP_WINDOW)) {
          spotifyTapPending = false;
          vibrate(200);  // Instant feedback before API call
          if (spotifyTapCount == 1) {
            // Single tap = play/pause
            spotifyPlayPause();
          } else if (spotifyTapCount == 2) {
            // Double tap = next track
            spotifyNextTrack();
          } else if (spotifyTapCount >= 3) {
            // Triple tap = previous track
            spotifyPrevTrack();
          }
          spotifyTapCount = 0;
          lastSpotifyPoll = millis();  // Reset poll timer after action
        }
      } else {
        // No WiFi - discard any pending taps silently
        if (spotifyTapPending) {
          spotifyTapPending = false;
          spotifyTapCount = 0;
        }
      }
      break;
      
    case POMODORO_SELECT: {
      // Pomodoro selection screen
      displayPomodoroSelect();
      
      // Auto-start if same option stays selected for 10 seconds (no touches)
      if (millis() - pomodoroSelectionTime >= 10000) {
        vibrate(200);
        pomodoroEndTime = millis() + ((unsigned long)pomodoroOptions[pomodoroSelected] * 60000UL);
        pomodoroPaused = false;
        currentState = POMODORO_RUNNING;
        stateStartTime = millis();
      }
      break;
    }
    
    case POMODORO_RUNNING:
      // Show countdown timer
      displayPomodoroCountdown();
      
      // Check if timer is done (only when not paused)
      if (!pomodoroPaused && millis() >= pomodoroEndTime) {
        // Timer complete! Move to celebration phase
        pomodoroPaused = false;
        currentState = POMODORO_DONE;
        stateStartTime = millis();
      }
      break;
    
    case POMODORO_DONE: {
      // Celebration phase - 5 seconds of happy + laugh + vibration
      eyes.setMood(HAPPY);
      
      // Laugh animation every 2 seconds (same pattern as affection)
      unsigned long celebCycle = stateTime % 2000;
      if (celebCycle < 50 || (celebCycle > 600 && celebCycle < 650) || (celebCycle > 1200 && celebCycle < 1250)) {
        eyes.anim_laugh();
      }
      
      // Vibration pulses: 100ms on, 100ms off (repeating)
      unsigned long celebPulse = stateTime % 200;
      if (celebPulse < 100) {
        digitalWrite(VIBRATION_PIN, HIGH);
      } else {
        digitalWrite(VIBRATION_PIN, LOW);
      }
      
      // After 5 seconds, return to idle
      if (stateTime >= 5000) {
        digitalWrite(VIBRATION_PIN, LOW);
        currentState = IDLE;
        eyes.setMood(DEFAULT);
        stateStartTime = millis();
      }
      break;
    }
    
    case SHOWING_DISTANCE:
      // Display distance reading continuously
      displayDistance();
      
      // Note: Tap to cycle to temperature display is handled in handleTouch()
      break;
      
    case SHOWING_TEMPERATURE:
      // Display temperature reading
      displayTemperature();
      
      // Note: Tap to cycle to humidity display is handled in handleTouch()
      break;
      
    case SHOWING_HUMIDITY:
      // Display humidity reading
      displayHumidity();
      
      // Note: Tap to exit is handled in handleTouch()
      break;
      
    case BRIGHTNESS_ADJUST:
      // Display brightness adjustment screen
      displayBrightness();
      // Note: Tap to cycle brightness is handled in handleTouch()
      break;

    case DISTANCE_ADJUST:
      // Display distance/presence settings screen
      displayDistanceAdjust();
      // Note: Tap to cycle options is handled in handleTouch()
      break;

    case BONKED: {
      // Show bonked animation with timing + vibration pulses synced with animation
      
      // Non-blocking vibration pattern: 6 pulses of 100ms with 50ms gaps, starting at 100ms
      if (stateTime >= 100 && stateTime < 1000) {
        // Calculate which vibration pulse we should be in
        unsigned long patternTime = stateTime - 100;  // Time since pattern started
        int currentPulse = patternTime / 150;  // Each pulse+gap = 150ms
        int withinPulse = patternTime % 150;    // Position within current pulse cycle
        
        // Turn vibration on/off based on position
        if (currentPulse < 6 && withinPulse < 100) {
          digitalWrite(VIBRATION_PIN, HIGH);
        } else {
          digitalWrite(VIBRATION_PIN, LOW);
        }
      }
      
      if (stateTime < 1100) {
        // Phase 1: Confused animation with vertical flicker - synced with vibration pulses
        eyes.setMood(TIRED);
        eyes.setVFlicker(true, 5);
      }
      else if (stateTime < 3000) {
          digitalWrite(VIBRATION_PIN, LOW);
          eyes.setVFlicker(false, 5);
          eyes.setMood(TIRED);
      }
      else if (stateTime < 6000) {
        // Phase 2: Show angry with CONTINUOUS vibration (3 seconds)
        eyes.setMood(ANGRY);
        digitalWrite(VIBRATION_PIN, HIGH);
      }
      else {
        // Phase 3: Recovery - turn off vibration
        digitalWrite(VIBRATION_PIN, LOW);
        eyes.setMood(DEFAULT);
        currentState = RECOVERING;
        stateStartTime = millis();
      }
      break;
    }
      
    case RECOVERING:
      // Slow return to normal
      if (stateTime < 50) {
        eyes.setMood(DEFAULT);
        eyes.setPosition(DEFAULT);
        eyes.open();
      }
      
      if (stateTime > 1000) {
        currentState = IDLE;
      }
      break;
    
    case BREAK_REMINDER:
      // Show break reminder for 5 seconds
      displayBreakReminder();
      
      // Green LED pulses with vibration for first 2s, then stays solid
      if (stateTime < 2000) {
        unsigned long pulsePos = stateTime % 300;
        if (pulsePos < 200) {
          digitalWrite(VIBRATION_PIN, HIGH);
          ledSetColor(0, 255, 0);
        } else {
          digitalWrite(VIBRATION_PIN, LOW);
          ledOff();
        }
      } else {
        digitalWrite(VIBRATION_PIN, LOW);
        ledSetColor(0, 255, 0);
      }
      
      // After 5 seconds, return to previous mode
      if (stateTime >= 5000) {
        digitalWrite(VIBRATION_PIN, LOW);
        ledOff();
        // Reset so another reminder fires in 1 hour
        breakReminderShown = false;
        continuousPresenceStart = millis();
        currentState = previousState;
        stateStartTime = millis();
      }
      break;
    
    case POSTURE_ALERT:
      // Show posture alert
      displayPostureAlert();
      
      // Red LED pulses with vibration for first 2s, then stays solid
      if (stateTime < 2000) {
        unsigned long pulsePos = stateTime % 250;
        if (pulsePos < 150) {
          digitalWrite(VIBRATION_PIN, HIGH);
          ledSetColor(255, 0, 0);
        } else {
          digitalWrite(VIBRATION_PIN, LOW);
          ledOff();
        }
      } else {
        digitalWrite(VIBRATION_PIN, LOW);
        ledSetColor(255, 0, 0);
      }
      
      // After 5 seconds, return to previous mode
      if (stateTime >= 5000) {
        digitalWrite(VIBRATION_PIN, LOW);
        ledOff();
        currentState = previousState;
        stateStartTime = millis();
      }
      break;
    
    case WATER_REMINDER:
      // Show water reminder
      displayWaterReminder();
      
      // Blue LED pulses with vibration for first 2s, then stays solid
      if (stateTime < 2000) {
        unsigned long pulsePos = stateTime % 300;
        if (pulsePos < 150) {
          digitalWrite(VIBRATION_PIN, HIGH);
          ledSetColor(0, 0, 255);
        } else {
          digitalWrite(VIBRATION_PIN, LOW);
          ledOff();
        }
      } else {
        digitalWrite(VIBRATION_PIN, LOW);
        ledSetColor(0, 0, 255);
      }
      
      // After 5 seconds, return to previous mode
      if (stateTime >= 5000) {
        digitalWrite(VIBRATION_PIN, LOW);
        ledOff();
        currentState = previousState;
        stateStartTime = millis();
      }
      break;

  }
}