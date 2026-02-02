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

// Idle animation state
int idleState = 0;


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
  // Check touch sensor
  checkTouch();
  
  // Smooth pupil tracking animation
  smoothPupilMovement();
  
  // If touch detected, show selected emotion
  if (touchDetected && millis() - lastTouchTime < 3000) {
    showEmotionByTouch();
  } else {
    // Organic idle behaviors
    if (millis() - lastIdleTime > idleInterval) {
      lastIdleTime = millis();
      idleInterval = random(2000, 5000);
      
      int behavior = random(0, 10);
      
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
  
  // Detect rising edge (touch event)
  if (currentTouch && !lastTouchState) {
    touchDetected = true;
    lastTouchTime = millis();
    
    // Cycle to next emotion
    currentEmotion++;
    if (currentEmotion > 4) {
      currentEmotion = 0;
    }
    
    Serial.print("Touch detected! Emotion: ");
    Serial.println(currentEmotion);
    
    delay(200); // Debounce
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
