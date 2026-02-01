/*
 * ESP32 Animated Eyes - Mochi Inspired
 * 
 * Pin Connections:
 * OLED GND  -> ESP32 GND
 * OLED VDD  -> ESP32 3.3V
 * OLED SCK  -> ESP32 GPIO 18 (HSPI CLK)
 * OLED SDA  -> ESP32 GPIO 23 (HSPI MOSI)
 * OLED RES  -> ESP32 GPIO 4 (Reset)
 * OLED DC   -> ESP32 GPIO 2 (Data/Command)
 * OLED CS   -> ESP32 GPIO 5 (Chip Select)
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

// Eye parameters
#define EYE_RADIUS 15
#define PUPIL_RADIUS 7
#define EYE_SPACING 30

// Eye positions
int leftEyeX = 34;
int leftEyeY = 32;
int rightEyeX = 94;
int rightEyeY = 32;

// Pupil positions (offset from center)
int pupilOffsetX = 0;
int pupilOffsetY = 0;

// Blink state
int blinkState = 0;
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 3000;

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
  
  randomSeed(analogRead(0));
}

void loop() {
  // Random behaviors
  int behavior = random(0, 6);
  
  switch(behavior) {
    case 0:
      lookAround();
      break;
    case 1:
      blinkEyes();
      delay(500);
      break;
    case 2:
      happyEyes();
      delay(2000);
      break;
    case 3:
      surprisedEyes();
      delay(1500);
      break;
    case 4:
      sleepyEyes();
      delay(2000);
      break;
    case 5:
      heartEyes();
      delay(2000);
      break;
  }
  
  // Normal eyes between behaviors
  drawNormalEyes();
  delay(random(500, 1500));
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
