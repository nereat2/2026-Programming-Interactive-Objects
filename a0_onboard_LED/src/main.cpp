/**
 * Animate falling white pixels on a 32x32 RGB LED matrix.
 */

#include <Arduino.h>
#include <PxMatrix.h>

// This is the pinout for the Feather ESP32 HUZZAH
// For other boards please refer to the PxMatrix library documentation
// and change the pinout accordingly.
#define P_LAT 22
#define P_OE 21
#define P_A 19
#define P_B 5
#define P_C 18
#define P_D 17
#define P_E 16 // required for 32x32 panel

// PxMATRIX display(32,32,P_LAT, P_OE,P_A,P_B,P_C,P_D);
// PxMATRIX display(64, 64, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);
PxMATRIX display(32, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

// This defines the 'on' color
uint16_t white = display.color565(255, 255, 255);

// Function to update the display
void display_updater() {
  display.display(70);
}

void setup() {
  display.begin(16);
  display.clearDisplay(true);
  display.setBrightness(100);
}

void loop() {
  // Shift everything down
  for (int y = 31; y > 0; y--) {
    for (int x = 0; x < 32; x++) {
      uint16_t color = display.getPixel(x, y - 1);
      display.drawPixel(x, y, color);
    }
  }

  // Add new random white pixels at the top to simulate rain
  for (int x = 0; x < 32; x++) {
    if (random(10) < 2) { // 20% chance of a new drop
      display.drawPixel(x, 0, white);
    } else {
      display.drawPixel(x, 0, display.color565(0, 0, 0)); // Clear the pixel
    }
  }

  delay(100); // Animation speed
}