/*
 * TTP223 Touch Sensor Test with OLED Display
 * 
 * Pin Connections:
 * OLED GND  -> ESP32 GND
 * OLED VDD  -> ESP32 3.3V
 * OLED SCK  -> ESP32 GPIO 18
 * OLED SDA  -> ESP32 GPIO 23
 * OLED RES  -> ESP32 GPIO 4
 * OLED DC   -> ESP32 GPIO 2
 * OLED CS   -> ESP32 GPIO 5
 * 
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
#define TOUCH_PIN   15

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

int touchCount = 0;
bool lastTouchState = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Touch Sensor Test Starting...");

  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  pinMode(TOUCH_PIN, INPUT);
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  
  Serial.println("Touch Sensor Test Ready!");
  showStartScreen();
}

void loop() {
  bool currentTouch = digitalRead(TOUCH_PIN);
  
  // Detect rising edge (new touch)
  if (currentTouch && !lastTouchState) {
    touchCount++;
    Serial.print("Touch detected! Count: ");
    Serial.println(touchCount);
    showTouched();
    delay(200); // Debounce
  }
  // Detect falling edge (release)
  else if (!currentTouch && lastTouchState) {
    Serial.println("Touch released");
    showNotTouched();
  }
  
  lastTouchState = currentTouch;
  delay(50);
}

void showStartScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 10);
  display.println("TOUCH");
  display.setCursor(10, 30);
  display.println("SENSOR");
  display.setTextSize(1);
  display.setCursor(30, 55);
  display.println("TEST MODE");
  display.display();
  delay(2000);
  showNotTouched();
}

void showTouched() {
  display.clearDisplay();
  
  // Big text
  display.setTextSize(3);
  display.setCursor(5, 15);
  display.println("TOUCH!");
  
  // Touch count
  display.setTextSize(2);
  display.setCursor(20, 45);
  display.print("Count: ");
  display.println(touchCount);
  
  // Visual indicator - filled rectangle
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  
  display.display();
}

void showNotTouched() {
  display.clearDisplay();
  
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("Waiting");
  display.setCursor(25, 30);
  display.println("Touch");
  display.setCursor(30, 50);
  display.println("Here!");
  
  // Draw touch indicator (empty rectangle)
  display.drawRect(0, 0, 128, 10, SSD1306_WHITE);
  
  // Show touch count
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Touches: ");
  display.println(touchCount);
  
  display.display();
}
