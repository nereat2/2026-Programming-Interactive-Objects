// Pinout configuration for the PicoDriver v.5.0
#include "common/pico_driver_v5_pinout.h"

#include <Arduino.h>
#include <SmartMatrix.h>
#include <math.h>

#define COLOR_DEPTH 24
#define TOTAL_WIDTH 32
#define TOTAL_HEIGHT 32
#define kRefreshDepth 24
#define kDmaBufferRows 2
#define kPanelType SM_PANELTYPE_HUB75_32ROW_MOD16SCAN
#define kMatrixOptions (SM_HUB75_OPTIONS_NONE)
#define kbgOptions (SM_BACKGROUND_OPTIONS_NONE)

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, TOTAL_WIDTH, TOTAL_HEIGHT, kRefreshDepth,
                             kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(bg, TOTAL_WIDTH, TOTAL_HEIGHT,
                                      COLOR_DEPTH, kbgOptions);

float time_val = 0;

// Simple pseudo-random noise function
float hash(float n) { return fmod(sin(n) * 43758.5453123f, 1.0f); }

// 2D Noise
float noise2d(float x, float y) {
  float fl_x = floor(x);
  float fl_y = floor(y);
  float fc_x = fmod(x, 1.0f);
  float fc_y = fmod(y, 1.0f);

  // Bilinear interpolation of hash
  float n1 = hash(fl_x + fl_y * 57.0f);
  float n2 = hash(fl_x + 1.0f + fl_y * 57.0f);
  float n3 = hash(fl_x + (fl_y + 1.0f) * 57.0f);
  float n4 = hash(fl_x + 1.0f + (fl_y + 1.0f) * 57.0f);

  float lx = fc_x * (3.0f - 2.0f * fc_x * fc_x); // Smoothstep
  float ly = fc_y * (3.0f - 2.0f * fc_y * fc_y);

  return n1 + (n2 - n1) * lx + (n3 - n1) * ly + (n1 - n2 - n3 + n4) * lx * ly;
}

void setup() {
  pinMode(PICO_LED_PIN, OUTPUT);
  digitalWrite(PICO_LED_PIN, 1);

  matrix.addLayer(&bg);
  matrix.setBrightness(120);
  matrix.begin();

  // Debug: Red Flash
  bg.fillScreen({255, 0, 0});
  bg.swapBuffers();
  delay(500);
}

void loop() {
  for (int y = 0; y < TOTAL_HEIGHT; y++) {
    for (int x = 0; x < TOTAL_WIDTH; x++) {

      // Generate cloud pattern
      float n = noise2d(x * 0.15f, y * 0.15f + time_val * 0.5f);

      // Map to Black & White (Halftone style)
      uint8_t val = (uint8_t)(constrain(n + 0.2f, 0.0f, 1.0f) * 255);

      // Hard contrast for comic book look
      if (val < 50)
        val = 0;
      else
        val = 255;

      bg.drawPixel(x, y, {val, val, val});
    }
  }
  time_val += 0.05f;
  bg.swapBuffers();
}
