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

void setup() {

	// On board LED (useful for debugging)
	pinMode(PICO_LED_PIN, OUTPUT);

	// Turn the on board LED on
	digitalWrite(PICO_LED_PIN, 1);

	bg.enableColorCorrection(true);
	matrix.addLayer(&bg);
	matrix.setBrightness(255);

	// Init the library and the matrix
	matrix.begin();

}

// http://www.pouet.net/prod.php?which=57245
// If you intend to reuse this shader, please add credits to 'Danilo Guanabara'

rgb24 shader(float x, float y, float time){
	float c[3] = {0, 0, 0};
	float l, z = time;
	
	for(int i = 0; i < 3; i++) {
		// Normalize coordinates to [0, 1]
		float uv_x = (x + 1.0) / 2.0;
		float uv_y = (y + 1.0) / 2.0;
		
		float p_x = uv_x;
		float p_y = uv_y;
		
		p_x -= 0.5;
		p_y -= 0.5;
		p_x *= (float)TOTAL_WIDTH / (float)TOTAL_HEIGHT;
		
		z += 0.07;
		l = sqrt(p_x * p_x + p_y * p_y);
		
		if (l > 0.0001) {
			float scale = (sin(z) + 1.0) * abs(sin(l * 9.0 - z - z));
			uv_x += (p_x / l) * scale;
			uv_y += (p_y / l) * scale;
		}
		
		// mod(uv, 1.0) - 0.5
		float mod_uv_x = uv_x - floor(uv_x) - 0.5;
		float mod_uv_y = uv_y - floor(uv_y) - 0.5;
		
		float len_mod = sqrt(mod_uv_x * mod_uv_x + mod_uv_y * mod_uv_y);
		if (len_mod > 0.0001) {
			c[i] = 0.01 / len_mod;
		}
	}
	
	uint8_t r = (uint8_t)min(255.0f, c[0] * 255.0f);
	uint8_t g = (uint8_t)min(255.0f, c[1] * 255.0f);
	uint8_t b = (uint8_t)min(255.0f, c[2] * 255.0f);
	
	return {r, g, b};
}

int frame = 0;

void loop() {

 float t = frame * 0.1;

	for (int j=0; j<TOTAL_HEIGHT; j++) {
		for (int i=0; i<TOTAL_WIDTH; i++) {

			// normalized coordinates of the pixels: 
			// instead of 0 to 31 we have -1.0 to 1.0			
			float x = (float) i / (TOTAL_WIDTH - 1) * 2.0 - 1.0;
			float y = (float) j / (TOTAL_HEIGHT - 1) * 2.0 - 1.0;
			
			rgb24 out = shader(x,y,t); 
			bg.drawPixel(i, j, out);
		}
	}

	bg.swapBuffers();
	frame++;
}