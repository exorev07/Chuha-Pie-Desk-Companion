/*
 * ESP32 Animated Eyes - Mochi Inspired with Touch Control
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
 * TTP223 Touch Sensor:
 * TTP223 VCC -> ESP32 3.3V
 * TTP223 GND -> ESP32 GND
 * TTP223 SIG -> ESP32 GPIO 15
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Pin definitions
#define OLED_MOSI   23
#define OLED_CLK    18
#define OLED_DC     2
#define OLED_CS     5
#define OLED_RESET  4
#define TOUCH_PIN   15  // TTP223 touch sensor

// Eye parameters
#define EYE_RADIUS 15
#define PUPIL_RADIUS 7
#define EYE_SPACING 30

// Eye positions
int leftEyeX = 34;
int leftEyeY = 32;
int rightEyeX = 94;
int rightEyeY = 32;

// Pupil positions (offset from center) - with smooth tracking
float pupilOffsetX = 0;
float pupilOffsetY = 0;
float targetPupilX = 0;
float targetPupilY = 0;

// Eye squash and stretch for organic animation
float leftEyeScaleX = 1.0;
float leftEyeScaleY = 1.0;
float rightEyeScaleX = 1.0;
float rightEyeScaleY = 1.0;

// Blink state
int blinkState = 0;
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 3000;
unsigned long lastIdleTime = 0;
unsigned long idleInterval = 2000;

// Touch sensor state
int currentEmotion = 0;  // 0=normal, 1=happy, 2=surprised, 3=sleepy, 4=heart
bool touchDetected = false;
bool lastTouchState = false;
unsigned long lastTouchTime = 0;
unsigned long touchStartTime = 0;
unsigned long touchDuration = 0;
bool isLongPress = false;
int affectionLevel = 0;  // Builds up during long press

// Idle animation state
int idleState = 0;
unsigned long lastRandomAction = 0;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void setup() {
  Serial.begin(115200);
  Serial.println("Animated Eyes Starting...");

  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  Serial.println("Eyes initialized!");
  display.clearDisplay();
  display.display();
  delay(500);
  
  // Setup touch sensor
  pinMode(TOUCH_PIN, INPUT);
  Serial.println("Touch sensor ready!");
  
  randomSeed(analogRead(0));
}

void loop() {
  // Check touch sensor continuously
  checkTouch();
  
  // Smooth pupil tracking animation
  smoothPupilMovement();
  
  // Handle long press affection
  if (isLongPress) {
    showAffectionResponse();
  }
  // If touch detected, show selected emotion
  else if (touchDetected && millis() - lastTouchTime < 2000) {
    showEmotionByTouch();
  } else {
    // Organic idle behaviors with more variety
    if (millis() - lastIdleTime > idleInterval) {
      lastIdleTime = millis();
      idleInterval = random(1500, 4000);
      
      int behavior = random(0, 15);  // More variety
      
      switch(behavior) {
        case 0:
        case 1:
          lookAroundSmooth();
          break;
        case 2:
          blinkEyesSmooth();
          break;
        case 3:
          curiousLook();
          break;
        case 4:
          happyBounce();
          break;
        case 5:
          excitedWiggle();
          break;
        case 6:
          shyLook();
          break;
        case 7:
          playfulWink();
          break;
        case 8:
          sneeze();  // Random sneeze!
          break;
        case 9:
          yawn();
          break;
        case 10:
          stretch();
          break;
        case 11:
          confusedTilt();
          break;
        case 12:
          giggle();
          break;
        default:
          // Just look around subtly
          targetPupilX = random(-4, 5);
          targetPupilY = random(-3, 4);
          break;
      }
    }
    
    // Auto blink occasionally
    if (millis() - lastBlinkTime > blinkInterval) {
      lastBlinkTime = millis();
      blinkInterval = random(2000, 5000);
      quickBlink();
    }
    
    // Draw eyes continuously
    drawNormalEyesSmooth();
    delay(20); // Smooth 50fps animation
  }
}

void checkTouch() {
  bool currentTouch = digitalRead(TOUCH_PIN);
  
  // Detect rising edge (touch started)
  if (currentTouch && !lastTouchState) {
    touchStartTime = millis();
    touchDetected = true;
    affectionLevel = 0;
    Serial.println("Touch started!");
  }
  
  // While touching, track duration
  if (currentTouch) {
    touchDuration = millis() - touchStartTime;
    
    // Long press detection (1+ seconds)
    if (touchDuration > 1000 && !isLongPress) {
      isLongPress = true;
      Serial.println("Long press detected! Getting excited...");
    }
    
    // Build up affection during long press
    if (isLongPress) {
      affectionLevel = min(100, (int)((touchDuration - 1000) / 30));  // 0-100 over ~3 seconds
    }
  }
  
  // Detect falling edge (touch released)
  if (!currentTouch && lastTouchState) {
    touchDuration = millis() - touchStartTime;
    lastTouchTime = millis();
    
    if (isLongPress) {
      Serial.println("Long press released! Showing big heart!");
      showBigHeartSequence();
      isLongPress = false;
      affectionLevel = 0;
    } else {
      // Short press - cycle emotions
      currentEmotion++;
      if (currentEmotion > 4) {
        currentEmotion = 0;
      }
      Serial.print("Short touch! Emotion: ");
      Serial.println(currentEmotion);
    }
    
    touchDuration = 0;
  }
  
  lastTouchState = currentTouch;
}

void showEmotionByTouch() {
  switch(currentEmotion) {
    case 0:
      drawNormalEyes();
      break;
    case 1:
      happyEyes();
      break;
    case 2:
      surprisedEyes();
      break;
    case 3:
      sleepyEyes();
      break;
    case 4:
      heartEyes();
      break;
  }
  delay(100);
}

void drawNormalEyes() {
  display.clearDisplay();
  
  // Left eye
  display.fillCircle(leftEyeX, leftEyeY, EYE_RADIUS, SSD1306_WHITE);
  display.fillCircle(leftEyeX + pupilOffsetX, leftEyeY + pupilOffsetY, 
                     PUPIL_RADIUS, SSD1306_BLACK);
  display.fillCircle(leftEyeX + pupilOffsetX - 3, leftEyeY + pupilOffsetY - 3, 
                     3, SSD1306_WHITE); // Light reflection
  
  // Right eye
  display.fillCircle(rightEyeX, rightEyeY, EYE_RADIUS, SSD1306_WHITE);
  display.fillCircle(rightEyeX + pupilOffsetX, rightEyeY + pupilOffsetY, 
                     PUPIL_RADIUS, SSD1306_BLACK);
  display.fillCircle(rightEyeX + pupilOffsetX - 3, rightEyeY + pupilOffsetY - 3, 
                     3, SSD1306_WHITE); // Light reflection
  
  display.display();
}

// Smooth animated eyes with organic movement
void drawNormalEyesSmooth() {
  display.clearDisplay();
  
  // Left eye with smooth scaling
  int leftRadiusX = EYE_RADIUS * leftEyeScaleX;
  int leftRadiusY = EYE_RADIUS * leftEyeScaleY;
  drawEllipse(leftEyeX, leftEyeY, leftRadiusX, leftRadiusY, true);
  
  // Pupil
  display.fillCircle(leftEyeX + (int)pupilOffsetX, leftEyeY + (int)pupilOffsetY, 
                     PUPIL_RADIUS, SSD1306_BLACK);
  display.fillCircle(leftEyeX + (int)pupilOffsetX - 3, leftEyeY + (int)pupilOffsetY - 3, 
                     2, SSD1306_WHITE); // Light reflection
  
  // Right eye with smooth scaling
  int rightRadiusX = EYE_RADIUS * rightEyeScaleX;
  int rightRadiusY = EYE_RADIUS * rightEyeScaleY;
  drawEllipse(rightEyeX, rightEyeY, rightRadiusX, rightRadiusY, true);
  
  // Pupil
  display.fillCircle(rightEyeX + (int)pupilOffsetX, rightEyeY + (int)pupilOffsetY, 
                     PUPIL_RADIUS, SSD1306_BLACK);
  display.fillCircle(rightEyeX + (int)pupilOffsetX - 3, rightEyeY + (int)pupilOffsetY - 3, 
                     2, SSD1306_WHITE); // Light reflection
  
  display.display();
}

// Smooth pupil tracking with easing
void smoothPupilMovement() {
  float easing = 0.15;
  pupilOffsetX += (targetPupilX - pupilOffsetX) * easing;
  pupilOffsetY += (targetPupilY - pupilOffsetY) * easing;
  
  // Reset scales smoothly
  leftEyeScaleX += (1.0 - leftEyeScaleX) * 0.1;
  leftEyeScaleY += (1.0 - leftEyeScaleY) * 0.1;
  rightEyeScaleX += (1.0 - rightEyeScaleX) * 0.1;
  rightEyeScaleY += (1.0 - rightEyeScaleY) * 0.1;
}

// Draw ellipse for organic eye shapes
void drawEllipse(int cx, int cy, int rx, int ry, bool fill) {
  if (rx == ry) {
    if (fill) display.fillCircle(cx, cy, rx, SSD1306_WHITE);
    else display.drawCircle(cx, cy, rx, SSD1306_WHITE);
    return;
  }
  
  for (int y = -ry; y <= ry; y++) {
    int x = rx * sqrt(1.0 - (float)(y * y) / (ry * ry));
    if (fill) {
      display.drawLine(cx - x, cy + y, cx + x, cy + y, SSD1306_WHITE);
    } else {
      display.drawPixel(cx + x, cy + y, SSD1306_WHITE);
      display.drawPixel(cx - x, cy + y, SSD1306_WHITE);
    }
  }
}

void lookAround() {
  // Look left
  animatePupilMovement(-5, 0, 300);
  delay(800);
  
  // Look right
  animatePupilMovement(5, 0, 300);
  delay(800);
  
  // Look up
  animatePupilMovement(0, -4, 300);
  delay(800);
  
  // Look down
  animatePupilMovement(0, 3, 300);
  delay(800);
  
  // Back to center
  animatePupilMovement(0, 0, 300);
}

// Smooth organic looking around
void lookAroundSmooth() {
  int moves[][2] = {{-5, 0}, {-5, -3}, {0, -4}, {5, -3}, {5, 0}, {0, 0}};
  
  for (int i = 0; i < 6; i++) {
    targetPupilX = moves[i][0];
    targetPupilY = moves[i][1];
    
    for (int j = 0; j < 30; j++) {
      smoothPupilMovement();
      drawNormalEyesSmooth();
      delay(20);
    }
  }
}

// Quick natural blink
void quickBlink() {
  for (int i = 0; i < 5; i++) {
    leftEyeScaleY = 1.0 - (i * 0.2);
    rightEyeScaleY = 1.0 - (i * 0.2);
    drawNormalEyesSmooth();
    delay(15);
  }
  
  delay(50);
  
  for (int i = 5; i >= 0; i--) {
    leftEyeScaleY = 1.0 - (i * 0.2);
    rightEyeScaleY = 1.0 - (i * 0.2);
    drawNormalEyesSmooth();
    delay(15);
  }
}

// Curious look animation
void curiousLook() {
  // Squash and stretch
  for (int i = 0; i < 10; i++) {
    leftEyeScaleX = 1.0 + sin(i * 0.3) * 0.15;
    leftEyeScaleY = 1.0 - sin(i * 0.3) * 0.15;
    rightEyeScaleX = leftEyeScaleX;
    rightEyeScaleY = leftEyeScaleY;
    
    targetPupilY = -4 + sin(i * 0.5) * 2;
    smoothPupilMovement();
    drawNormalEyesSmooth();
    delay(50);
  }
}

// Happy bounce animation
void happyBounce() {
  for (int i = 0; i < 3; i++) {
    // Bounce up
    for (int j = 0; j < 8; j++) {
      leftEyeScaleY = 1.0 + sin(j * 0.4) * 0.2;
      rightEyeScaleY = leftEyeScaleY;
      drawNormalEyesSmooth();
      delay(30);
    }
  }
  happyEyes();
  delay(800);
}

// Excited wiggle
void excitedWiggle() {
  for (int i = 0; i < 20; i++) {
    targetPupilX = sin(i * 0.5) * 4;
    targetPupilY = cos(i * 0.6) * 3;
    smoothPupilMovement();
    
    leftEyeScaleX = 1.0 + sin(i * 0.4) * 0.1;
    rightEyeScaleX = 1.0 + sin(i * 0.4 + 0.5) * 0.1;
    
    drawNormalEyesSmooth();
    delay(40);
  }
  targetPupilX = 0;
  targetPupilY = 0;
}

// Shy look away
void shyLook() {
  targetPupilX = -6;
  targetPupilY = 2;
  
  for (int i = 0; i < 30; i++) {
    smoothPupilMovement();
    drawNormalEyesSmooth();
    delay(30);
  }
  
  quickBlink();
  delay(500);
  
  targetPupilX = 0;
  targetPupilY = 0;
}

// Playful wink
void playfulWink() {
  // Wink left eye
  for (int i = 0; i < 8; i++) {
    leftEyeScaleY = 1.0 - (i * 0.125);
    targetPupilX = 3;
    smoothPupilMovement();
    drawNormalEyesSmooth();
    delay(30);
  }
  
  delay(200);
  
  for (int i = 8; i >= 0; i--) {
    leftEyeScaleY = 1.0 - (i * 0.125);
    drawNormalEyesSmooth();
    delay(30);
  }
  
  targetPupilX = 0;
}

void animatePupilMovement(int targetX, int targetY, int duration) {
  int steps = 10;
  int startX = pupilOffsetX;
  int startY = pupilOffsetY;
  
  for(int i = 0; i <= steps; i++) {
    pupilOffsetX = map(i, 0, steps, startX, targetX);
    pupilOffsetY = map(i, 0, steps, startY, targetY);
    drawNormalEyes();
    delay(duration / steps);
  }
}

void blinkEyes() {
  // Close eyes
  for(int i = 0; i <= EYE_RADIUS; i += 3) {
    display.clearDisplay();
    
    // Draw closing eyelids
    display.fillCircle(leftEyeX, leftEyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillRect(leftEyeX - EYE_RADIUS, leftEyeY - i, 
                     EYE_RADIUS * 2, i * 2, SSD1306_BLACK);
    
    display.fillCircle(rightEyeX, rightEyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillRect(rightEyeX - EYE_RADIUS, rightEyeY - i, 
                     EYE_RADIUS * 2, i * 2, SSD1306_BLACK);
    
    display.display();
    delay(30);
  }
  
  delay(100);
  
  // Open eyes
  for(int i = EYE_RADIUS; i >= 0; i -= 3) {
    display.clearDisplay();
    
    display.fillCircle(leftEyeX, leftEyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillRect(leftEyeX - EYE_RADIUS, leftEyeY - i, 
                     EYE_RADIUS * 2, i * 2, SSD1306_BLACK);
    
    display.fillCircle(rightEyeX, rightEyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillRect(rightEyeX - EYE_RADIUS, rightEyeY - i, 
                     EYE_RADIUS * 2, i * 2, SSD1306_BLACK);
    
    display.display();
    delay(30);
  }
  
  drawNormalEyes();
}

// Smooth organic blink
void blinkEyesSmooth() {
  for (int i = 0; i < 10; i++) {
    leftEyeScaleY = 1.0 - (i * 0.1);
    rightEyeScaleY = 1.0 - (i * 0.1);
    drawNormalEyesSmooth();
    delay(20);
  }
  
  delay(80);
  
  for (int i = 10; i >= 0; i--) {
    leftEyeScaleY = 1.0 - (i * 0.1);
    rightEyeScaleY = 1.0 - (i * 0.1);
    drawNormalEyesSmooth();
    delay(20);
  }
}

void happyEyes() {
  display.clearDisplay();
  
  // Draw happy arcs (eyes closed in smile)
  for(int i = 0; i < 5; i++) {
    display.drawLine(leftEyeX - 15, leftEyeY + i, 
                     leftEyeX - 5, leftEyeY - 8 + i, SSD1306_WHITE);
    display.drawLine(leftEyeX - 5, leftEyeY - 8 + i, 
                     leftEyeX + 5, leftEyeY - 8 + i, SSD1306_WHITE);
    display.drawLine(leftEyeX + 5, leftEyeY - 8 + i, 
                     leftEyeX + 15, leftEyeY + i, SSD1306_WHITE);
    
    display.drawLine(rightEyeX - 15, rightEyeY + i, 
                     rightEyeX - 5, rightEyeY - 8 + i, SSD1306_WHITE);
    display.drawLine(rightEyeX - 5, rightEyeY - 8 + i, 
                     rightEyeX + 5, rightEyeY - 8 + i, SSD1306_WHITE);
    display.drawLine(rightEyeX + 5, rightEyeY - 8 + i, 
                     rightEyeX + 15, rightEyeY + i, SSD1306_WHITE);
  }
  
  display.display();
}

void surprisedEyes() {
  display.clearDisplay();
  
  // Large circular eyes
  display.fillCircle(leftEyeX, leftEyeY, EYE_RADIUS + 3, SSD1306_WHITE);
  display.fillCircle(leftEyeX, leftEyeY, PUPIL_RADIUS + 2, SSD1306_BLACK);
  display.fillCircle(leftEyeX - 2, leftEyeY - 2, 2, SSD1306_WHITE);
  
  display.fillCircle(rightEyeX, rightEyeY, EYE_RADIUS + 3, SSD1306_WHITE);
  display.fillCircle(rightEyeX, rightEyeY, PUPIL_RADIUS + 2, SSD1306_BLACK);
  display.fillCircle(rightEyeX - 2, rightEyeY - 2, 2, SSD1306_WHITE);
  
  // Raised eyebrows
  display.fillRect(leftEyeX - 12, leftEyeY - 25, 24, 3, SSD1306_WHITE);
  display.fillRect(rightEyeX - 12, rightEyeY - 25, 24, 3, SSD1306_WHITE);
  
  display.display();
}

void sleepyEyes() {
  // Droopy eyelids animation
  for(int i = 0; i < 3; i++) {
    display.clearDisplay();
    
    // Half-closed eyes
    display.fillCircle(leftEyeX, leftEyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillRect(leftEyeX - EYE_RADIUS, leftEyeY - 10, 
                     EYE_RADIUS * 2, 10, SSD1306_BLACK);
    
    display.fillCircle(rightEyeX, rightEyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillRect(rightEyeX - EYE_RADIUS, rightEyeY - 10, 
                     EYE_RADIUS * 2, 10, SSD1306_BLACK);
    
    display.display();
    delay(800);
    
    drawNormalEyes();
    delay(400);
  }
}

void heartEyes() {
  display.clearDisplay();
  
  // Draw hearts in place of eyes
  drawHeart(leftEyeX, leftEyeY, 10);
  drawHeart(rightEyeX, rightEyeY, 10);
  
  display.display();
}

void drawHeart(int x, int y, int size) {
  // Simple heart shape
  display.fillCircle(x - size/2, y - size/3, size/2, SSD1306_WHITE);
  display.fillCircle(x + size/2, y - size/3, size/2, SSD1306_WHITE);
  display.fillTriangle(x - size, y, x, y + size, x + size, y, SSD1306_WHITE);
}

// ============ NEW ANIMATIONS FOR DESK COMPANION ============

// Show affection response during long press
void showAffectionResponse() {
  display.clearDisplay();
  
  // Eyes get progressively happier and bigger
  int excitement = map(affectionLevel, 0, 100, 0, 10);
  
  // Draw increasingly happy eyes
  for(int i = 0; i < 3 + excitement/3; i++) {
    display.drawLine(leftEyeX - 15, leftEyeY + i, 
                     leftEyeX - 5, leftEyeY - 5 - excitement + i, SSD1306_WHITE);
    display.drawLine(leftEyeX - 5, leftEyeY - 5 - excitement + i, 
                     leftEyeX + 5, leftEyeY - 5 - excitement + i, SSD1306_WHITE);
    display.drawLine(leftEyeX + 5, leftEyeY - 5 - excitement + i, 
                     leftEyeX + 15, leftEyeY + i, SSD1306_WHITE);
    
    display.drawLine(rightEyeX - 15, rightEyeY + i, 
                     rightEyeX - 5, rightEyeY - 5 - excitement + i, SSD1306_WHITE);
    display.drawLine(rightEyeX - 5, rightEyeY - 5 - excitement + i, 
                     rightEyeX + 5, rightEyeY - 5 - excitement + i, SSD1306_WHITE);
    display.drawLine(rightEyeX + 5, rightEyeY - 5 - excitement + i, 
                     rightEyeX + 15, rightEyeY + i, SSD1306_WHITE);
  }
  
  // Show hearts floating up as affection builds
  if (affectionLevel > 20) {
    int numHearts = affectionLevel / 30 + 1;
    for (int i = 0; i < numHearts; i++) {
      int heartY = 60 - (affectionLevel * (i + 1) / 10) % 60;
      drawHeart(10 + i * 30, heartY, 5);
    }
  }
  
  display.display();
  delay(30);
}

// Big heart sequence after long press
void showBigHeartSequence() {
  // Huge heart explosion animation
  for (int size = 5; size < 35; size += 3) {
    display.clearDisplay();
    drawHeart(64, 32, size);
    display.display();
    delay(40);
  }
  
  // Pulse the heart
  for (int i = 0; i < 3; i++) {
    // Grow
    for (int size = 35; size < 40; size += 2) {
      display.clearDisplay();
      drawHeart(64, 32, size);
      display.display();
      delay(50);
    }
    // Shrink
    for (int size = 40; size > 35; size -= 2) {
      display.clearDisplay();
      drawHeart(64, 32, size);
      display.display();
      delay(50);
    }
  }
  
  // Show happy eyes with the heart
  for (int i = 0; i < 50; i++) {
    display.clearDisplay();
    drawHeart(64, 20, 15);
    
    // Super happy eyes
    display.drawLine(40, 45, 45, 42, SSD1306_WHITE);
    display.drawLine(45, 42, 50, 42, SSD1306_WHITE);
    display.drawLine(50, 42, 55, 45, SSD1306_WHITE);
    
    display.drawLine(73, 45, 78, 42, SSD1306_WHITE);
    display.drawLine(78, 42, 83, 42, SSD1306_WHITE);
    display.drawLine(83, 42, 88, 45, SSD1306_WHITE);
    
    display.display();
    delay(40);
  }
  
  delay(500);
}

// Sneeze animation
void sneeze() {
  Serial.println("*Achoo!*");
  
  // Build up
  for (int i = 0; i < 10; i++) {
    display.clearDisplay();
    
    // Eyes squinting
    leftEyeScaleY = 0.3;
    rightEyeScaleY = 0.3;
    targetPupilY = 3;
    
    drawEllipse(leftEyeX, leftEyeY, EYE_RADIUS, EYE_RADIUS * leftEyeScaleY, true);
    drawEllipse(rightEyeX, rightEyeY, EYE_RADIUS, EYE_RADIUS * rightEyeScaleY, true);
    
    display.display();
    delay(50);
  }
  
  // Sneeze!
  for (int i = 0; i < 5; i++) {
    display.clearDisplay();
    
    // Eyes shut tight
    display.fillRect(leftEyeX - 15, leftEyeY - 2, 30, 4, SSD1306_WHITE);
    display.fillRect(rightEyeX - 15, rightEyeY - 2, 30, 4, SSD1306_WHITE);
    
    // Sneeze particles
    for (int j = 0; j < 8; j++) {
      int px = 64 + random(-30, 30) + i * 10;
      int py = 32 + random(-20, 20);
      display.fillCircle(px, py, 1, SSD1306_WHITE);
    }
    
    display.display();
    delay(80);
  }
  
  delay(300);
  
  // Recover
  leftEyeScaleY = 1.0;
  rightEyeScaleY = 1.0;
  targetPupilY = 0;
  quickBlink();
}

// Yawn animation
void yawn() {
  Serial.println("*Yawn*");
  
  // Eyes getting droopy
  for (int i = 0; i < 15; i++) {
    display.clearDisplay();
    
    leftEyeScaleY = 1.0 - (i * 0.05);
    rightEyeScaleY = 1.0 - (i * 0.05);
    
    drawEllipse(leftEyeX, leftEyeY, EYE_RADIUS, EYE_RADIUS * leftEyeScaleY, true);
    drawEllipse(rightEyeX, rightEyeY, EYE_RADIUS, EYE_RADIUS * rightEyeScaleY, true);
    
    // Open mouth
    int mouthWidth = i * 2;
    int mouthHeight = i;
    display.drawCircle(64, 45, mouthWidth/2, SSD1306_WHITE);
    
    display.display();
    delay(60);
  }
  
  delay(400);
  
  // Close mouth and blink
  for (int i = 15; i >= 0; i--) {
    display.clearDisplay();
    
    leftEyeScaleY = 1.0 - (i * 0.05);
    rightEyeScaleY = 1.0 - (i * 0.05);
    
    drawEllipse(leftEyeX, leftEyeY, EYE_RADIUS, EYE_RADIUS * leftEyeScaleY, true);
    drawEllipse(rightEyeX, rightEyeY, EYE_RADIUS, EYE_RADIUS * rightEyeScaleY, true);
    
    display.display();
    delay(40);
  }
  
  leftEyeScaleY = 1.0;
  rightEyeScaleY = 1.0;
}

// Stretch animation
void stretch() {
  Serial.println("*Stretch*");
  
  // Squash down
  for (int i = 0; i < 8; i++) {
    display.clearDisplay();
    
    leftEyeScaleY = 1.0 - (i * 0.08);
    rightEyeScaleY = 1.0 - (i * 0.08);
    leftEyeScaleX = 1.0 + (i * 0.05);
    rightEyeScaleX = 1.0 + (i * 0.05);
    
    drawEllipse(leftEyeX, leftEyeY + i, EYE_RADIUS * leftEyeScaleX, EYE_RADIUS * leftEyeScaleY, true);
    drawEllipse(rightEyeX, rightEyeY + i, EYE_RADIUS * rightEyeScaleX, EYE_RADIUS * rightEyeScaleY, true);
    
    display.display();
    delay(50);
  }
  
  delay(100);
  
  // Stretch up!
  for (int i = 8; i >= -5; i--) {
    display.clearDisplay();
    
    leftEyeScaleY = 1.0 + (abs(i - 3) * 0.1);
    rightEyeScaleY = 1.0 + (abs(i - 3) * 0.1);
    leftEyeScaleX = 1.0 - (abs(i - 3) * 0.05);
    rightEyeScaleX = 1.0 - (abs(i - 3) * 0.05);
    
    int yOffset = i;
    drawEllipse(leftEyeX, leftEyeY + yOffset, EYE_RADIUS * leftEyeScaleX, EYE_RADIUS * leftEyeScaleY, true);
    drawEllipse(rightEyeX, rightEyeY + yOffset, EYE_RADIUS * rightEyeScaleX, EYE_RADIUS * rightEyeScaleY, true);
    
    display.display();
    delay(50);
  }
  
  // Reset
  leftEyeScaleX = 1.0;
  leftEyeScaleY = 1.0;
  rightEyeScaleX = 1.0;
  rightEyeScaleY = 1.0;
}

// Confused head tilt
void confusedTilt() {
  Serial.println("*Confused*");
  
  for (int angle = 0; angle < 20; angle += 2) {
    display.clearDisplay();
    
    // Tilt eyes
    int leftTiltY = sin(angle * 0.1) * 3;
    int rightTiltY = -sin(angle * 0.1) * 3;
    
    drawEllipse(leftEyeX, leftEyeY + leftTiltY, EYE_RADIUS * 0.9, EYE_RADIUS, true);
    display.fillCircle(leftEyeX - 2, leftEyeY + leftTiltY, PUPIL_RADIUS - 2, SSD1306_BLACK);
    
    drawEllipse(rightEyeX, rightEyeY + rightTiltY, EYE_RADIUS * 0.9, EYE_RADIUS, true);
    display.fillCircle(rightEyeX + 2, rightEyeY + rightTiltY, PUPIL_RADIUS - 2, SSD1306_BLACK);
    
    // Question mark appears
    if (angle > 10) {
      display.setTextSize(2);
      display.setCursor(100, 15);
      display.print("?");
    }
    
    display.display();
    delay(50);
  }
  
  delay(800);
  
  // Tilt back
  for (int angle = 20; angle >= 0; angle -= 2) {
    display.clearDisplay();
    
    int leftTiltY = sin(angle * 0.1) * 3;
    int rightTiltY = -sin(angle * 0.1) * 3;
    
    drawEllipse(leftEyeX, leftEyeY + leftTiltY, EYE_RADIUS * 0.9, EYE_RADIUS, true);
    display.fillCircle(leftEyeX - 2, leftEyeY + leftTiltY, PUPIL_RADIUS - 2, SSD1306_BLACK);
    
    drawEllipse(rightEyeX, rightEyeY + rightTiltY, EYE_RADIUS * 0.9, EYE_RADIUS, true);
    display.fillCircle(rightEyeX + 2, rightEyeY + rightTiltY, PUPIL_RADIUS - 2, SSD1306_BLACK);
    
    display.display();
    delay(50);
  }
}

// Giggle animation
void giggle() {
  Serial.println("*Giggle*");
  
  for (int i = 0; i < 5; i++) {
    // Eyes close in joy
    display.clearDisplay();
    
    for(int j = 0; j < 3; j++) {
      display.drawLine(leftEyeX - 12, leftEyeY + j, 
                       leftEyeX - 4, leftEyeY - 6 + j, SSD1306_WHITE);
      display.drawLine(leftEyeX - 4, leftEyeY - 6 + j, 
                       leftEyeX + 4, leftEyeY - 6 + j, SSD1306_WHITE);
      display.drawLine(leftEyeX + 4, leftEyeY - 6 + j, 
                       leftEyeX + 12, leftEyeY + j, SSD1306_WHITE);
      
      display.drawLine(rightEyeX - 12, rightEyeY + j, 
                       rightEyeX - 4, rightEyeY - 6 + j, SSD1306_WHITE);
      display.drawLine(rightEyeX - 4, rightEyeY - 6 + j, 
                       rightEyeX + 4, rightEyeY - 6 + j, SSD1306_WHITE);
      display.drawLine(rightEyeX + 4, rightEyeY - 6 + j, 
                       rightEyeX + 12, rightEyeY + j, SSD1306_WHITE);
    }
    
    // Bouncing motion
    int bounce = abs((i % 4) - 2);
    
    display.display();
    delay(120);
    
    // Quick open
    drawNormalEyesSmooth();
    delay(80);
  }
}
