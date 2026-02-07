/*
 * Desk Companion with RoboEyes - Mochi Inspired
 * 
 * Hardware:
 * - ESP32 DevKit V1
 * - 7-Pin SSD1306 OLED (128x64, SPI)
 * - TTP223 Capacitive Touch Sensor
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
 * Features:
 * - Long press (>2s): Shows happy mood with laugh animation
 * - Single tap: Displays current time from NTP
 * - Double tap: Dizzy/confused animation with vertical flicker and angry mood
 * - Auto blink and idle movements
 * - WiFi + NTP for accurate time display
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>
#include <WiFi.h>
#include <time.h>

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

// ============ ANIMATION STATE ============
enum State {
  IDLE,
  LONG_PRESS_BUILDING,
  SHOWING_AFFECTION,
  WAITING_FOR_SECOND_TAP,
  SHOWING_TIME,
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
  
  // Setup touch sensor
  pinMode(TOUCH_PIN, INPUT);
}

// ============ MAIN LOOP ============
void loop() {
  handleTouch();
  updateState();
  
  // Only update eyes when not showing time
  if (currentState != SHOWING_TIME) {
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
        // Tap while showing time - go back to eyes
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
      // Normal idle behavior - RoboEyes handles this automatically
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
