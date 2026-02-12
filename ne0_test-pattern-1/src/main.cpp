// Pinout configuration for the PicoDriver v.5.0
#include "common/pico_driver_v5_pinout.h"

#include <Arduino.h>
#include <SmartMatrix.h>
#include <math.h>

#define COLOR_DEPTH 24 // valid: 24, 48
#define TOTAL_WIDTH                                                            \
  32 // Size of the total (chained) with of the matrix/matrices
#define TOTAL_HEIGHT                                                           \
  32 // Size of the total (chained) height of the matrix/matrices
#define kRefreshDepth 24 // Valid: 24, 36, 48
#define kDmaBufferRows 2 // Reduced for Pico memory safety
#define kPanelType SM_PANELTYPE_HUB75_32ROW_MOD16SCAN
#define kMatrixOptions (SM_HUB75_OPTIONS_NONE)
#define kbgOptions (SM_BACKGROUND_OPTIONS_NONE)

// SmartMatrix setup & buffer alloction
SMARTMATRIX_ALLOCATE_BUFFERS(matrix, TOTAL_WIDTH, TOTAL_HEIGHT, kRefreshDepth,
                             kDmaBufferRows, kPanelType, kMatrixOptions);

// Layers - Only background needed now
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(bg, TOTAL_WIDTH, TOTAL_HEIGHT,
                                      COLOR_DEPTH, kbgOptions);

// --- Constants & Tuning ---
const int FPS = 60;
const int FRAME_DELAY_MS = 1000 / FPS;

// Baseline settings
const int BASE_RGB_OFFSET = 1;      // pixels
const int MACRO_BLOCK_SIZE = 8;     // 4 or 8
const int SNOW_PROBABILITY = 20;    // out of 1000
const int SCANLINE_INTENSITY = 180; // 0-255 (amount to keep)

// Crack settings
const int CRACK_INTERVAL_MIN = 3000; // ms
const int CRACK_INTERVAL_MAX = 6000; // ms
const int CRACK_DURATION_MIN = 300;  // ms
const int CRACK_DURATION_MAX = 1000; // ms
const int CRACK_DISPLACEMENT = 6;    // pixels
const int MAX_CRACKS = 3;

// --- Fast RNG ---
// Xorshift or simple linear congruential for speed
uint32_t rng_state = 12345;
uint32_t fastRandom() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

// Returns 0..max-1
uint32_t fastRandomRange(uint32_t max) { return fastRandom() % max; }

// Deterministic pixel hash 0..255
uint8_t pixelHash(int x, int y, uint32_t t) {
  uint32_t h = (x * 374761393) ^ (y * 668265263) ^ t;
  h = (h ^ (h >> 13)) * 1274126177;
  return h >> 24;
}

// --- Crack Struct ---
struct Crack {
  bool active;
  uint32_t startTime;
  uint32_t duration;
  int y_start;
  float slope;   // x displacement per y
  int direction; // -1 (left shift top) or 1 (right shift top)
  int displacement_max;
};

Crack cracks[MAX_CRACKS];
uint32_t lastCrackTime = 0;
uint32_t nextCrackInterval = 3000;

// --- Globals ---
uint32_t frameCount = 0;
float rollingBarPos = 0.0f;

void spawnCrack(uint32_t now) {
  for (int i = 0; i < MAX_CRACKS; i++) {
    if (!cracks[i].active) {
      cracks[i].active = true;
      cracks[i].startTime = now;
      cracks[i].duration =
          CRACK_DURATION_MIN +
          fastRandomRange(CRACK_DURATION_MAX - CRACK_DURATION_MIN);
      cracks[i].y_start = fastRandomRange(TOTAL_HEIGHT);
      cracks[i].slope = (fastRandomRange(20) - 10) / 10.0f;
      cracks[i].direction = (fastRandomRange(2) == 0) ? -1 : 1;
      cracks[i].displacement_max = CRACK_DISPLACEMENT + fastRandomRange(3);
      break;
    }
  }
}

// --- Core Rendering ---

// Get the "Signal" color at a specific coordinate (simulating the TV image)
// For this effect, the signal is a noisy gray field
void getSignalColor(int x, int y, uint32_t t, uint8_t &r, uint8_t &g,
                    uint8_t &b) {
  // Wrap coordinates
  int wx = (x + TOTAL_WIDTH * 10) % TOTAL_WIDTH;
  int wy = (y + TOTAL_HEIGHT * 10) % TOTAL_HEIGHT;

  // Base signal is perceptually gray noise
  uint8_t n = pixelHash(wx, wy, t / 5); // Change noise every few frames

  // Perlin-ish low freq background to make it look like a "signal" and not just
  // white noise We'll fake it with coordinate waves
  uint8_t signal = 100 + (n / 4); // Base gray level 100-163

  // Add some "features" or dark blobs
  if (pixelHash(wx / 4, wy / 4, t / 50) < 50) {
    signal /= 2;
  }

  r = signal;
  g = signal;
  b = signal;
}

float time_val = 0;

// Simple pseudo-random noise function
float hash(float n) { return fmod(sin(n) * 43758.5453123f, 1.0f); }

float noise(float p) {
  float fl = floor(p);
  float fc = fmod(p, 1.0f);
  return hash(fl) * (1.0f - fc) + hash(fl + 1.0f) * fc;
}

// 2D Noise
float noise2d(float x, float y) { return hash(x + y * 57.0f); }

void setup() {
  pinMode(PICO_LED_PIN, OUTPUT);
  digitalWrite(PICO_LED_PIN, 1);
  // bg.enableColorCorrection(true); // Optional, sometimes raw looks more
  // "glitchy"

  matrix.addLayer(&bg);
  matrix.setBrightness(180); // Bright for that CRT feel
  matrix.begin();

  // Debug: Flash Red to verify display is working
  bg.fillScreen({255, 0, 0});
  bg.swapBuffers();
  delay(1000);

  // Init cracks
  for (int i = 0; i < MAX_CRACKS; i++)
    cracks[i].active = false;
  lastCrackTime = millis();
}

void loop() {
  // Glitch intensity factor (periodic but chaotic)
  float glitch_trigger = sin(time_val * 0.5f) + sin(time_val * 2.2f);
  bool is_glitching = (glitch_trigger > 1.2f) || (random(0, 100) > 98);

  float aberration = 0.0f;
  if (is_glitching) {
    aberration =
        (random(2, 8) / 10.0f) * sin(time_val * 20.0f); // Massive shift
  } else {
    aberration = 0.05f * sin(time_val * 3.0f); // Subtle shift
  }

  for (int y = 0; y < TOTAL_HEIGHT; y++) {

    // Scanline effect (dark bands moving down)
    float scanline = sin(y * 0.5f - time_val * 10.0f);
    scanline = (scanline + 1.0f) * 0.5f; // 0..1
    scanline = pow(scanline, 0.5f);      // Sharpen a bit

    for (int x = 0; x < TOTAL_WIDTH; x++) {

      // Coordinate distortion (Wave)
      float wave_y = y + sin(time_val * 2.0f + x * 0.2f) * 2.0f;
      if (is_glitching) {
        wave_y += (random(-5, 5)) * 0.5f; // Jitter
      }

      // 1. Base Noise Layer (TV Static)
      // High frequency noise
      float n = noise2d(x * 10.0f + time_val * 50.0f, y * 10.0f);

      // Make it greyish, not purely random
      // Bias towards light grey as requested
      float base_val = 0.7f + n * 0.3f;

      // Add heavy dark noise spots ("dead pixels" or dust)
      if (random(0, 1000) > 995)
        base_val = 0.0f;

      // 2. Sample RGB with offsets (Chromatic Aberration)
      // We are sampling the "pattern" which is just the noise + some wave

      // We need a "source signal" to distort.
      // Let's create a simple vertical bar or shape that gets glitched, or just
      // pure noise. User asked for "light gray background with some darker
      // noise". So the "signal" IS the noise.

      // To make RGB split visible, the noise must differ per channel offset.

      float r_n = noise2d((x + aberration * 5.0f) * 0.5f, wave_y * 0.5f);
      float g_n = noise2d(x * 0.5f, wave_y * 0.5f);
      float b_n = noise2d((x - aberration * 5.0f) * 0.5f, wave_y * 0.5f);

      // Map noise to intensity
      // Light gray background (high baseline) + noise
      float r = 0.6f + r_n * 0.4f;
      float g = 0.6f + g_n * 0.4f;
      float b = 0.6f + b_n * 0.4f;

      // Apply Scanlines
      // Darken rows
      float scan_intensity = 0.7f + 0.3f * scanline;
      r *= scan_intensity;
      g *= scan_intensity;
      b *= scan_intensity;

      // Vertical Sync loss effect (roll)
      if (is_glitching && random(0, 5) == 0) {
        r *= 0.5f; // Darken random bands
      }

      // Final Color
      // Boost saturation if glitching
      if (is_glitching) {
        // Make it pop with primary colors
        if (r > g && r > b) {
          r = 1.0f;
          g *= 0.5f;
          b *= 0.5f;
        } else if (g > r && g > b) {
          g = 1.0f;
          r *= 0.5f;
          b *= 0.5f;
        } else {
          b = 1.0f;
          r *= 0.5f;
          g *= 0.5f;
        }
      }

      bg.drawPixel(
          x, y, {(uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255)});
    }
  }

  time_val += 0.05f;
  bg.swapBuffers();
}