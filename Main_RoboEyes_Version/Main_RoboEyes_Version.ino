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
 * Features:
 * - Long press (>2s): Shows happy mood with laugh animation
 * - Single tap: Cycles through time, distance, and climate displays
 * - Double tap: Dizzy/confused animation with vertical flicker and angry mood
 * - Auto blink and idle movements
 * - WiFi + NTP for accurate time display
 * - Presence detection: Shows TIRED when alone, greets when someone sits down
 * - Temperature and humidity monitoring
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
#define TRIG_PIN    26  // HC-SR04 Trigger
#define ECHO_PIN    27  // HC-SR04 Echo
#define DHT_PIN     14  // DHT11 Data

// ============ DHT11 SENSOR CONFIG ============
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ============ DISTANCE SENSOR CONFIG ============
#define DETECTION_DISTANCE 15  // Distance in cm to detect presence (adjust as needed)
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

// ============ PRESENCE DETECTION ============
bool personPresent = false;
bool wasPersonPresent = false;
unsigned long lastDistanceCheck = 0;
unsigned long greetingStartTime = 0;
unsigned long lastDistanceDisplayUpdate = 0;
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
  SHOWING_DISTANCE,
  SHOWING_CLIMATE,
  BONKED,
  RECOVERING
};

State currentState = IDLE;
unsigned long stateStartTime = 0;

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
}

// ============ MAIN LOOP ============
void loop() {
  checkPresence();
  handleTouch();
  updateState();
  
  // Only update eyes when not showing info displays
  if (currentState != SHOWING_TIME && currentState != SHOWING_DISTANCE && currentState != SHOWING_CLIMATE) {
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
  
  // Detect if someone is within detection range
  if (distance > 0 && distance < DETECTION_DISTANCE) {
    personPresent = true;
  } else {
    personPresent = false;
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
    display.println("Time unavailable :(");
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
    display.setCursor(20, 30);
    if (distance < 100) {
      display.print(distance, 1);  // Show 1 decimal for < 100cm
    } else {
      display.print((int)distance);  // No decimals for larger values
    }
    display.setTextSize(2);
    display.println(" cm");
  } else {
    // Out of range
    display.setTextSize(2);
    display.setCursor(35, 30);
    display.println("- - -");
  }
  
  display.display();
}

void displayClimate() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    display.setTextSize(1);
    display.setCursor(15, 25);
    display.println("Sensor Error!" );
    display.display();
    return;
  }
  
  // Temperature section
  display.setTextSize(1);
  display.setCursor(10, 5);
  display.print("Temperature:");
  display.setTextSize(2);
  display.setCursor(30, 20);
  display.print(temperature, 1);
  display.setTextSize(1);
  display.print(" C");
  
  // Humidity section
  display.setTextSize(1);
  display.setCursor(10, 40);
  display.print("Humidity:");
  display.setTextSize(2);
  display.setCursor(30, 50);
  display.print(humidity, 0);
  display.setTextSize(1);
  display.print(" %");
  
  display.display();
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
        currentState = SHOWING_AFFECTION;
        stateStartTime = millis();
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
        lastTapTime = 0;
      } else if (currentState == SHOWING_TIME) {
        // Tap while showing time - show distance
        currentState = SHOWING_DISTANCE;
        lastTapTime = 0;
      } else if (currentState == SHOWING_DISTANCE) {
        // Tap while showing distance - show climate
        currentState = SHOWING_CLIMATE;
        lastTapTime = 0;
      } else if (currentState == SHOWING_CLIMATE) {
        // Tap while showing climate - go back to eyes
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
      
    case SHOWING_AFFECTION:
      // Show happy with laugh animation - multiple laughs
      eyes.setMood(HAPPY);
      if (stateTime < 50 || (stateTime > 800 && stateTime < 850) || (stateTime > 1600 && stateTime < 1650)) {
        eyes.anim_laugh();
      }
      
      // Show for 3 seconds with multiple laughs
      if (stateTime > 3000 || !digitalRead(TOUCH_PIN)) {
        currentState = RECOVERING;
        stateStartTime = millis();
      }
      break;
      
    case WAITING_FOR_SECOND_TAP:
      // Wait for potential second tap
      if (stateTime > 500) {
        // Timeout - it was a single tap, show time
        currentState = SHOWING_TIME;
        stateStartTime = millis();
      }
      break;
      
    case SHOWING_TIME:
      // Display current time continuously
      displayCurrentTime();
      
      // Note: Tap to cycle to distance display is handled in handleTouch()
      break;
      
    case SHOWING_DISTANCE:
      // Display distance reading continuously
      displayDistance();
      
      // Note: Tap to cycle to climate display is handled in handleTouch()
      break;
      
    case SHOWING_CLIMATE:
      // Display temperature and humidity
      displayClimate();
      
      // Note: Tap to exit is handled in handleTouch()
      break;
      
    case BONKED:
      // Show bonked animation with timing
      if (stateTime < 1000) {
        // Phase 1: Confused animation
        if (stateTime < 200) {
          eyes.setMood(TIRED);
          eyes.setVFlicker(true, 5);
        }
      }
      else if (stateTime < 3000) {
          eyes.setVFlicker(false, 5);
          eyes.setMood(TIRED);
      }
      else if (stateTime < 8000) {
        // Phase 2: Show angry (no flicker)
        if (stateTime < 7000) {
          eyes.setMood(ANGRY);
        }
      }
      else {
        // Phase 3: Recovery
        eyes.setMood(DEFAULT);
        currentState = RECOVERING;
        stateStartTime = millis();
      }
      break;
      
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
