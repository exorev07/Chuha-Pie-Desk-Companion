/*
 * Desk Companion with RoboEyes - Mochi Inspired
 * 
 * Hardware:
 * - ESP32 DevKit V1
 * - 7-Pin SSD1306 OLED (128x64, SPI)
 * - TTP223 Capacitive Touch Sensor
 * - HC-SR04 Ultrasonic Distance Sensor
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
 * Features:
 * - Long press (>2s): Shows happy mood with laugh animation
 * - Single tap: Cycles through time, distance, and climate displays
 * - Double tap: Dizzy/confused animation with vertical flicker and angry mood
 * - Auto blink and idle movements
 * - WiFi + NTP for accurate time display
 * - Presence detection: Shows TIRED when alone, greets when someone sits down
 * - Temperature and humidity monitoring
 * - Haptic feedback on touch interactions
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <DHT.h>
#include <FluxGarage_RoboEyes.h>

// ============ WIFI CREDENTIALS ============
const char* WIFI_SSID = "Virus.exe";
const char* WIFI_PASSWORD = "Exorev@3727";

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

// ============ DHT11 SENSOR CONFIG ============
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ============ DISTANCE SENSOR CONFIG ============
#define DETECTION_DISTANCE 50  // Distance in cm to detect presence (adjust as needed)
#define DISTANCE_CHECK_INTERVAL 500  // Check distance every 500ms

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
unsigned long lastAffectionVibration = 0;  // Track vibration timing for affection mode
int bonkVibrationPulse = 0;                // Track which pulse we're on for bonk pattern

// ============ PRESENCE DETECTION ============
bool personPresent = false;
bool wasPersonPresent = false;
unsigned long lastDistanceCheck = 0;
unsigned long greetingStartTime = 0;
unsigned long lastDistanceDisplayUpdate = 0;
bool rawPresenceDetected = false;           // Raw sensor reading
unsigned long presenceChangeTime = 0;       // When raw detection last changed
#define PRESENCE_GONE_DELAY 5000            // 5s before marking absent
#define PRESENCE_ARRIVE_DELAY 700           // 700ms before marking present
enum GreetingState {
  NO_GREETING,
  GREETING_CURIOUS,
  GREETING_HAPPY,
  GREETING_DONE
};
GreetingState greetingState = NO_GREETING;

// ============ ANIMATION STATE ============
enum State {
  IDLE,
  LONG_PRESS_BUILDING,
  SHOWING_AFFECTION,
  WAITING_FOR_SECOND_TAP,
  SHOWING_TIME,
  POMODORO_SELECT,
  POMODORO_RUNNING,
  POMODORO_DONE,
  SHOWING_DISTANCE,
  SHOWING_TEMPERATURE,
  SHOWING_HUMIDITY,
  BONKED,
  RECOVERING
};

State currentState = IDLE;
unsigned long stateStartTime = 0;

// ============ POMODORO CONFIG ============
const int pomodoroOptions[] = {30, 45, 60, 90, 120};  // Minutes
const char* pomodoroLabels[] = {"30 Min", "45 Min", "1 Hr", "1.5 Hr", "2 Hr"};
int pomodoroSelected = 0;               // Currently highlighted option (0-4)
unsigned long pomodoroSelectionTime = 0; // When current option was selected
unsigned long pomodoroEndTime = 0;       // When the timer should end
bool pomodoroTouchHandled = false;       // Prevent multiple triggers per long press
bool pomodoroPaused = false;             // Whether the timer is paused
unsigned long pomodoroPausedRemaining = 0; // Remaining time when paused (ms)

// ============ SETUP ============
void setup() {
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    for(;;);
  }
  
  display.clearDisplay();
  display.display();
  
  // Initialize RoboEyes
  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, MAX_FPS);
  eyes.setAutoblinker(true, 3, 2);  // Auto blink every 3-5 seconds
  eyes.setIdleMode(true, 2, 3);     // Idle movements every 2-5 seconds
  
  // Configure eye appearance
  eyes.setWidth(45, 45);
  eyes.setHeight(30, 30);
  eyes.setBorderradius(10, 10);
  eyes.setSpacebetween(15);
  
  // Show startup message
  showStartupMessage();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Wake up eyes
  wakeUpEyes();
  
  // Initialize DHT sensor
  dht.begin();
  
  // Setup touch sensor
  pinMode(TOUCH_PIN, INPUT);
  
  // Setup ultrasonic sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Setup vibration motor (simple digitalWrite for better motor spin-up)
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);
}

// ============ MAIN LOOP ============
void loop() {
  checkPresence();
  handleTouch();
  updateState();
  
  // Only update eyes when not showing info displays
  if (currentState != SHOWING_TIME && currentState != SHOWING_DISTANCE && currentState != SHOWING_TEMPERATURE && currentState != SHOWING_HUMIDITY && currentState != POMODORO_SELECT && currentState != POMODORO_RUNNING) {
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
  delay(5000);
  
  display.clearDisplay();
  display.display();
}

// ============ WAKE UP ANIMATION ============
void wakeUpEyes() {
  // Wake up sequence
  eyes.close();
  delay(500);
  eyes.drawEyes();
  delay(1000);
  
  for(int i = 0; i < 3; i++) {
    eyes.open();
    eyes.drawEyes();
    delay(200);
    eyes.close();
    eyes.drawEyes();
    delay(200);
  }
  
  eyes.open();
  eyes.setMood(HAPPY);
  for(int i = 0; i < 20; i++) {
    eyes.drawEyes();
    delay(50);
  }
  
  eyes.setMood(DEFAULT);
  eyes.setPosition(DEFAULT);
  
  // Let eyes settle to center position before entering main loop
  for(int i = 0; i < 30; i++) {
    eyes.update();
    delay(33);  // ~30fps for 1 second
  }
}

// ============ DISTANCE MEASUREMENT ============
float getDistance() {
  // Clear the trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Send 10 microsecond pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read the echo
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  
  // Calculate distance in cm with decimal precision
  float distance = duration * 0.034 / 2;
  
  return distance;
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
  
  // Get raw sensor reading
  bool currentlyDetected = (distance > 0 && distance < DETECTION_DISTANCE);
  
  // Track when raw detection state changed
  if (currentlyDetected != rawPresenceDetected) {
    rawPresenceDetected = currentlyDetected;
    presenceChangeTime = millis();
  }
  
  // Debounced presence: require sustained detection/absence
  if (rawPresenceDetected && !personPresent) {
    // Someone detected but not yet confirmed — wait 1 second
    if (millis() - presenceChangeTime >= PRESENCE_ARRIVE_DELAY) {
      personPresent = true;
    }
  } else if (!rawPresenceDetected && personPresent) {
    // No one detected but still marked present — wait 10 seconds
    if (millis() - presenceChangeTime >= PRESENCE_GONE_DELAY) {
      personPresent = false;
    }
  }
  
  // Check for state transitions
  if (personPresent && !wasPersonPresent) {
    // Person just arrived - start greeting sequence
    greetingState = GREETING_CURIOUS;
    greetingStartTime = millis();
  } else if (!personPresent && wasPersonPresent) {
    // Person left - reset greeting
    greetingState = NO_GREETING;
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
  display.setCursor(0, 10);
  display.println("Connecting WiFi!");
  display.display();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    display.print(".");
    display.display();
    attempts++;
  }
  
  display.clearDisplay();
  if (WiFi.status() == WL_CONNECTED) {
    display.setCursor(0, 10);
    display.println("WiFi Connected!");
    display.setCursor(0, 25);
    display.println("Syncing time...");
    display.display();
    
    // Configure time
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    delay(2000);
    
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Time synced!");
    display.display();
    delay(1000);
  } else {
    display.setCursor(0, 10);
    display.println("WiFi Failed!");
    display.setCursor(0, 25);
    display.println("Check credentials");
    display.display();
    delay(3000);
  }
  
  display.clearDisplay();
  display.display();
}

// ============ TIME DISPLAY ============
void displayCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // If time fetch fails, show error
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 25);
    display.println("Time unavailable!");
    display.display();
    return;
  }
  
  // Format time as HH:MM:SS
  char timeStr[10];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  
  // Format date as DD-MM-YYYY
  char dateStr[15];
  strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", &timeinfo);
  
  // Display time
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Large time display
  display.setTextSize(2);
  display.setCursor(17, 15);
  display.println(timeStr);
  
  // Date below
  display.setTextSize(1);
  display.setCursor(35, 40);
  display.println(dateStr);
  
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
  display.println("Distance:");
  
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
  display.println("Temperature:");
  
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
  display.println("Humidity:");
  
  // Large humidity display
  display.setTextSize(3);
  display.setCursor(45, 30);
  display.print(humidity, 0);
  display.setTextSize(2);
  display.println("%");
  
  display.display();
}

// ============ POMODORO DISPLAY ============
void displayPomodoroSelect() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setTextSize(1);
  display.setCursor(10, 2);
  display.println("Pomodoro:");
  
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
  display.drawRect(4, 55, 120, 7, SSD1306_WHITE);
  display.fillRect(4, 55, barWidth, 7, SSD1306_WHITE);
  
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

// ============ TOUCH HANDLING ============
void handleTouch() {
  bool currentTouch = digitalRead(TOUCH_PIN);
  
  // Rising edge - touch started
  if (currentTouch && !lastTouchState) {
    touchStartTime = millis();
    isLongPress = false;
  }
  
  // While touching
  if (currentTouch) {
    touchDuration = millis() - touchStartTime;
    
    // Pomodoro select: hold cycles options every 600ms
    if (currentState == POMODORO_SELECT) {
      if (touchDuration >= 600 && !isLongPress) {
        isLongPress = true;  // First cycle at 600ms
        pomodoroSelected = (pomodoroSelected + 1) % 5;
        pomodoroSelectionTime = millis();  // Reset 10s auto-start countdown
        lastAffectionVibration = millis();  // Reuse as repeat timer
        vibrate(150);
      }
      // Keep cycling every 800ms while held
      if (isLongPress && (millis() - lastAffectionVibration >= 800)) {
        pomodoroSelected = (pomodoroSelected + 1) % 5;
        pomodoroSelectionTime = millis();
        lastAffectionVibration = millis();
        vibrate(150);
      }
    }
    // Pomodoro running: long press exits
    else if (currentState == POMODORO_RUNNING) {
      if (touchDuration >= 2000 && !isLongPress) {
        isLongPress = true;
        vibratePattern(3, 200, 200);
        pomodoroPaused = false;
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
          lastAffectionVibration = millis();  // Track for periodic vibrations
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
    else if (touchDuration < 300) {
      // Quick tap detected
      unsigned long timeSinceLastTap = millis() - lastTapTime;
      
      if (timeSinceLastTap < 500 && currentState == WAITING_FOR_SECOND_TAP) {
        // DOUBLE TAP!
        currentState = BONKED;
        stateStartTime = millis();
        bonkVibrationPulse = 0;  // Reset pulse counter for vibration pattern
        lastTapTime = 0;
      } else if (currentState == SHOWING_TIME) {
        // Tap while showing time - show pomodoro selection
        vibrate(200);
        currentState = POMODORO_SELECT;
        pomodoroSelected = 0;
        pomodoroSelectionTime = millis();
        pomodoroTouchHandled = false;
        stateStartTime = millis();
        lastTapTime = 0;
      } else if (currentState == POMODORO_SELECT) {
        // Tap while in pomodoro select - move forward to distance
        vibrate(200);
        currentState = SHOWING_DISTANCE;
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
          pomodoroPausedRemaining = pomodoroEndTime - millis();
          pomodoroPaused = true;
        }
        lastTapTime = 0;
      } else if (currentState == SHOWING_DISTANCE) {
        // Tap while showing distance - show temperature
        vibrate(200);  // Medium pulse for screen cycle
        currentState = SHOWING_TEMPERATURE;
        lastTapTime = 0;
      } else if (currentState == SHOWING_TEMPERATURE) {
        // Tap while showing temperature - show humidity
        vibrate(200);  // Medium pulse for screen cycle
        currentState = SHOWING_HUMIDITY;
        lastTapTime = 0;
      } else if (currentState == SHOWING_HUMIDITY) {
        // Tap while showing humidity - go back to eyes
        vibrate(200);  // Medium pulse for screen cycle
        currentState = IDLE;
        eyes.setMood(DEFAULT);
        eyes.setPosition(DEFAULT);
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
      // Handle presence-based behavior
      if (!personPresent) {
        // Nobody detected - show tired
        eyes.setMood(TIRED);
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
      unsigned long pulseTime = stateTime % 2000;  // Loop every 2 seconds
      // 10 pulses: 100ms on, 100ms off (each cycle is 200ms)
      int currentPulse = pulseTime / 200;       // Which pulse cycle we're in
      int withinPulse = pulseTime % 200;        // Position within current cycle
      
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
      
    case SHOWING_TIME:
      // Display current time continuously
      displayCurrentTime();
      
      // Note: Tap to cycle to distance display is handled in handleTouch()
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
          digitalWrite(VIBRATION_PIN, HIGH);  // Pulse on
        } else {
          digitalWrite(VIBRATION_PIN, LOW);   // Gap or done
        }
      }
      
      if (stateTime < 1100) {
        // Phase 1: Confused animation with vertical flicker - synced with vibration pulses
        eyes.setMood(TIRED);
        eyes.setVFlicker(true, 5);
      }
      else if (stateTime < 3000) {
          digitalWrite(VIBRATION_PIN, LOW);  // Ensure vibration is off
          eyes.setVFlicker(false, 5);
          eyes.setMood(TIRED);
      }
      else if (stateTime < 6000) {
        // Phase 2: Show angry with CONTINUOUS vibration (3 seconds)
        eyes.setMood(ANGRY);
        // Turn on continuous vibration during angry phase
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
  }
}
