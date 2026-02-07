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
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

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
unsigned long lastTouchEndTime = 0;
int tapCount = 0;
bool isLongPress = false;

// ============ ANIMATION STATE ============
enum State {
  IDLE,
  LONG_PRESS_BUILDING,
  SHOWING_HEART,
  WAITING_FOR_SECOND_TAP,
  RANDOM_MOOD,
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
  
  // Setup touch sensor
  pinMode(TOUCH_PIN, INPUT);
  
  // Welcome animation
  welcomeAnimation();
}

// ============ MAIN LOOP ============
void loop() {
  handleTouch();
  updateState();
  eyes.update();
}

// ============ WELCOME ANIMATION ============
void welcomeAnimation() {
  // Startup message
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
    // Very long press - show heart! (>2s)
    else if (touchDuration >= 2000) {
      if (!isLongPress) {
        isLongPress = true;
        currentState = SHOWING_HEART;
        stateStartTime = millis();
      }
    }
  }
  
  // Falling edge - touch released
  if (!currentTouch && lastTouchState) {
    touchDuration = millis() - touchStartTime;
    lastTouchEndTime = millis();
    
    // Handle different touch types
    if (isLongPress) {
      // Long press was shown, do nothing (already showing heart)
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
      // Show progressively happier eyes with excitement
      showAffectionBuildup();
      
      // Return to idle if touch released
      if (!digitalRead(TOUCH_PIN)) {
        currentState = IDLE;
        eyes.setMood(DEFAULT);
        eyes.setPosition(DEFAULT);
      }
      break;
      
    case SHOWING_HEART:
      // Display big heart
      displayBigHeart();
      
      // Show for 3 seconds
      if (stateTime > 3000 || !digitalRead(TOUCH_PIN)) {
        currentState = RECOVERING;
        stateStartTime = millis();
      }
      break;
      
    case WAITING_FOR_SECOND_TAP:
      // Wait for potential second tap
      if (stateTime > 500) {
        // Timeout - it was a single tap
        currentState = RANDOM_MOOD;
        stateStartTime = millis();
        triggerRandomMood();
      }
      break;
      
    case RANDOM_MOOD:
      // Show random expression
      if (stateTime == 0 || stateTime < 50) {
        triggerRandomMood();
      }
      
      // Return to idle after 2 seconds
      if (stateTime > 2000) {
        currentState = IDLE;
        eyes.setMood(DEFAULT);
        eyes.setPosition(DEFAULT);
      }
      break;
      
    case BONKED:
      // Show bonked animation with timing
      if (stateTime < 2000) {
        // Phase 1: Confused animation
        if (stateTime < 100) {
          eyes.setMood(TIRED);
          eyes.anim_confused();
        }
      }
      else if (stateTime < 6000) {
        // Phase 2: Show angry (no flicker)
        if (stateTime < 5000) {
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

// ============ AFFECTION BUILDUP ============
void showAffectionBuildup() {
  // Show progressively happier eyes with excitement
  eyes.setMood(HAPPY);
}

// ============ BIG HEART DISPLAY ============
void displayBigHeart() {
  // Show happy state
  eyes.setMood(HAPPY);
}

// ============ RANDOM MOOD TRIGGER ============
void triggerRandomMood() {
  int mood = random(0, 12);
  
  switch(mood) {
    case 0:
      eyes.setMood(HAPPY);
      eyes.close(0, 1);  // Close right eye
      delay(300);
      eyes.open(0, 1);
      break;
      
    case 1:
      eyes.setMood(DEFAULT);
      eyes.setWidth(50, 50);
      eyes.setHeight(35, 35);
      delay(1000);
      eyes.setWidth(45, 45);
      eyes.setHeight(30, 30);
      break;
      
    case 2:
      eyes.setMood(HAPPY);
      eyes.anim_laugh();
      break;
      
    case 3:
      eyes.setMood(DEFAULT);
      eyes.anim_confused();
      break;
      
    case 4:
      eyes.setMood(TIRED);
      delay(1500);
      break;
      
    case 5:
      eyes.setPosition(W);
      delay(800);
      eyes.setPosition(DEFAULT);
      break;
      
    case 6:
      eyes.setPosition(E);
      delay(800);
      eyes.setPosition(DEFAULT);
      break;
      
    case 7:
      eyes.setPosition(N);
      delay(800);
      eyes.setPosition(DEFAULT);
      break;
      
    case 8:
      eyes.setMood(ANGRY);
      delay(1000);
      break;
      
    case 9:
      eyes.setCuriosity(true);
      eyes.setPosition(NE);
      delay(500);
      eyes.setPosition(NW);
      delay(500);
      eyes.setCuriosity(false);
      eyes.setPosition(DEFAULT);
      break;
      
    case 10:
      eyes.setPosition(SE);
      eyes.blink();
      delay(1000);
      eyes.setPosition(DEFAULT);
      break;
      
    case 11:
      eyes.setSweat(true);
      delay(2000);
      eyes.setSweat(false);
      break;
  }
}
