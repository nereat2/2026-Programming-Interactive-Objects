/**
 * Control a single LED of a RGB matrix, directly from the controller.
 *
 * The SmartMatrix library offers many tools (and examples) to display graphics,
 * animations and texts.
 * Dependencies (and docs):
 * https://github.com/pixelmatix/SmartMatrix
 *
 * Fork of the library that allows control of the special 32x32 matrix
 * https://github.com/Kameeno/SmartMatrix
 */

// Pinout configuration for the PicoDriver v.5.0
#include "common/pico_driver_v5_pinout.h"

#include <Arduino.h>
#include <SmartMatrix.h>

#define COLOR_DEPTH 24   // valid: 24, 48
#define TOTAL_WIDTH 32   // Size of the total (chained) with of the matrix/matrices
#define TOTAL_HEIGHT 32  // Size of the total (chained) height of the matrix/matrices
#define kRefreshDepth 24 // Valid: 24, 36, 48
#define kDmaBufferRows 4 // Valid: 2-4
#define kPanelType SM_PANELTYPE_HUB75_32ROW_32COL_MOD8SCAN // custom
#define kMatrixOptions (SM_HUB75_OPTIONS_NONE)
#define kbgOptions (SM_BACKGROUND_OPTIONS_NONE)

// SmartMatrix setup & buffer alloction
SMARTMATRIX_ALLOCATE_BUFFERS(matrix, TOTAL_WIDTH, TOTAL_HEIGHT, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);

// A single background layer "bg"
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(bg, TOTAL_WIDTH, TOTAL_HEIGHT, COLOR_DEPTH, kbgOptions);

static inline float clampf(float x, float lo, float hi) {
	return (x < lo) ? lo : ((x > hi) ? hi : x);
}

static inline float fractf(float x) {
	return x - floorf(x);
}

static inline float hash11(float x) {
	return fractf(sinf(x * 127.1f + 311.7f) * 43758.5453f);
}

float h[TOTAL_WIDTH][TOTAL_HEIGHT] = {0.0f};
float vfield[TOTAL_WIDTH][TOTAL_HEIGHT] = {0.0f};

float waveK = 0.20f;
float waveDamp = 0.985f;
float renderGain = 2.3f;
float dropStrength = 1.0f;
int dropRadius = 2;
int dropBurst = 1;

int32_t demoDropTick = -1;

void dropAt(int cx, int cy, float strength, int radius, int burstCount) {
	radius = constrain(radius, 1, 5);
	burstCount = constrain(burstCount, 1, 8);
	float falloff = 2.2f / (float)(radius * radius);
	float jitter = radius * 0.45f;

	for (int b = 0; b < burstCount; b++) {
		float seed = (float)millis() * 0.001f + (float)(b * 17 + cx * 5 + cy * 3);
		int jc = (int)roundf((hash11(seed * 1.37f + 2.1f) - 0.5f) * 2.0f * jitter);
		int jr = (int)roundf((hash11(seed * 1.93f + 9.4f) - 0.5f) * 2.0f * jitter);
		int px = constrain(cx + jc, 1, TOTAL_WIDTH - 2);
		int py = constrain(cy + jr, 1, TOTAL_HEIGHT - 2);

		for (int dx = -radius; dx <= radius; dx++) {
			for (int dy = -radius; dy <= radius; dy++) {
				int x = px + dx;
				int y = py + dy;
				if (x < 1 || x > TOTAL_WIDTH - 2 || y < 1 || y > TOTAL_HEIGHT - 2) continue;
				float r2 = (float)(dx * dx + dy * dy);
				float w = expf(-r2 * falloff);
				vfield[x][y] += strength * w;
			}
		}
	}
}

void setup() {
	// On board LED (useful for debugging)
	pinMode(PICO_LED_PIN, OUTPUT);

	// Turn the on board LED on
	digitalWrite(PICO_LED_PIN, 1);

	bg.enableColorCorrection(true);
	matrix.addLayer(&bg);
	matrix.setBrightness(255);
	matrix.begin();
}

uint32_t frame = 0;

void loop() {
	const float t = frame * 0.06f;

	// Demo trigger: remove this when external interaction is wired in.
	int32_t tick = (int32_t)floorf(t / 2.0f);
	if (tick != demoDropTick) {
		demoDropTick = tick;
		int cx = 2 + (int)(hash11((float)tick * 13.71f + 1.0f) * (TOTAL_WIDTH - 4));
		int cy = 2 + (int)(hash11((float)tick * 29.37f + 2.0f) * (TOTAL_HEIGHT - 4));
		dropAt(cx, cy, dropStrength, dropRadius, dropBurst);
	}

	for (int x = 1; x < TOTAL_WIDTH - 1; x++) {
		for (int y = 1; y < TOTAL_HEIGHT - 1; y++) {
			float lap = h[x - 1][y] + h[x + 1][y] + h[x][y - 1] + h[x][y + 1] - 4.0f * h[x][y];
			vfield[x][y] = (vfield[x][y] + waveK * lap) * waveDamp;
		}
	}

	for (int x = 1; x < TOTAL_WIDTH - 1; x++) {
		for (int y = 1; y < TOTAL_HEIGHT - 1; y++) {
			h[x][y] += vfield[x][y];
		}
	}

	for (int j = 0; j < TOTAL_HEIGHT; j++) {
		for (int i = 0; i < TOTAL_WIDTH; i++) {
			int xm1 = (i > 0) ? (i - 1) : i;
			int xp1 = (i < TOTAL_WIDTH - 1) ? (i + 1) : i;
			int ym1 = (j > 0) ? (j - 1) : j;
			int yp1 = (j < TOTAL_HEIGHT - 1) ? (j + 1) : j;

			float gx = h[xp1][j] - h[xm1][j];
			float gy = h[i][yp1] - h[i][ym1];
			float shade = clampf(0.5f + renderGain * (gx * 0.5f + gy * 0.25f), 0.0f, 1.0f);
			uint8_t gray = (uint8_t)(shade * 255.0f + 0.5f);
			bg.drawPixel(i, j, {gray, gray, gray});
		}
	}

	bg.swapBuffers();
	frame++;
}
