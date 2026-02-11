/**
 * Simple example with motion: oscillators
 */

// Pinout configuration for the PicoDriver v.5.0
#include "common/pico_driver_v5_pinout.h"

#include <Arduino.h>
#include <SmartMatrix.h>

#define COLOR_DEPTH 24 // valid: 24, 48
#define TOTAL_WIDTH                                                            \
  32 // Size of the total (chained) with of the matrix/matrices
#define TOTAL_HEIGHT                                                           \
  32 // Size of the total (chained) height of the matrix/matrices
#define kRefreshDepth 24 // Valid: 24, 36, 48
#define kDmaBufferRows 2 // Reduced for Pico memory safety
#define kPanelType SM_PANELTYPE_HUB75_32ROW_32COL_MOD8SCAN // custom
#define kMatrixOptions (SM_HUB75_OPTIONS_NONE)
#define kbgOptions (SM_BACKGROUND_OPTIONS_NONE)

// Set scrolling options to 0 if library-specific constant is missing
#define kScrollOptions 0

// SmartMatrix setup & buffer alloction
SMARTMATRIX_ALLOCATE_BUFFERS(matrix, TOTAL_WIDTH, TOTAL_HEIGHT, kRefreshDepth,
                             kDmaBufferRows, kPanelType, kMatrixOptions);

// Layers
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(bg, TOTAL_WIDTH, TOTAL_HEIGHT,
                                      COLOR_DEPTH, kbgOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, TOTAL_WIDTH, TOTAL_HEIGHT,
                                     COLOR_DEPTH, kScrollOptions);

float time_val = 0;

void setup() {
  pinMode(PICO_LED_PIN, OUTPUT);
  digitalWrite(PICO_LED_PIN, 1);
  bg.enableColorCorrection(true);

  matrix.addLayer(&bg);
  matrix.addLayer(&scrollingLayer);

  matrix.setBrightness(50);
  matrix.begin();

  scrollingLayer.setMode(wrapForward);
  scrollingLayer.setSpeed(30);
  scrollingLayer.setFont(font5x7);
  scrollingLayer.setColor({255, 255, 255}); // White text
  scrollingLayer.start("SEA", -1);          // Scroll forever
}

void loop() {
  for (int y = 0; y < TOTAL_HEIGHT; y++) {
    for (int x = 0; x < TOTAL_WIDTH; x++) {
      // 1. Calculate Wave Intensity
      float scale = 0.55f;
      float val = sin(x * scale + time_val * 2.0f) +
                  sin(y * scale + time_val * 1.5f) +
                  sin((x + y) * scale * 0.8f + time_val * 1.2f) +
                  sin(sqrt(x * x + y * y) * scale * 0.4f - time_val * 1.8f);

      float normalized = (val + 2.5f) / 5.0f;
      normalized = pow(constrain(normalized, 0.0f, 1.0f), 1.6f);

      // 2. Color Logic
      rgb24 color;
      if (normalized < 0.4f) {
        float t = normalized / 0.4f;
        color.red = 0;
        color.green = (uint8_t)(30 * t);
        color.blue = (uint8_t)(20 + 80 * t);
      } else if (normalized < 0.85f) {
        float t = (normalized - 0.4f) / 0.45f;
        color.red = (uint8_t)(40 * t);
        color.green = (uint8_t)(30 + 190 * t);
        color.blue = (uint8_t)(100 + 155 * t);
      } else {
        float t = (normalized - 0.85f) / 0.15f;
        color.red = (uint8_t)(155 + 100 * t);
        color.green = (uint8_t)(220 + 35 * t);
        color.blue = 255;
      }

      bg.drawPixel(x, y, color);
    }
  }

  time_val += 0.08f;
  bg.swapBuffers();
}
