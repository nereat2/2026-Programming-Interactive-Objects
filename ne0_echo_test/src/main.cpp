// Pinout configuration for the PicoDriver v.5.0
#include "common/pico_driver_v5_pinout.h"

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SmartMatrix.h>
#include <WiFi.h>
#include <math.h>

// --- Matrix Config ---
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

// --- Network Config ---
const char *ssid = "ESP32-Matrix";
const char *password = "password123";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- Framebuffer for Mirroring ---
// 32 * 32 * 3 bytes (RGB)
uint8_t framebuffer[TOTAL_WIDTH * TOTAL_HEIGHT * 3];

// Helper to update both Matrix and Framebuffer
void drawPixelMirrored(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= TOTAL_WIDTH || y < 0 || y >= TOTAL_HEIGHT)
    return;

  // Update SmartMatrix
  bg.drawPixel(x, y, {r, g, b});

  // Update Framebuffer
  int idx = (y * TOTAL_WIDTH + x) * 3;
  framebuffer[idx] = r;
  framebuffer[idx + 1] = g;
  framebuffer[idx + 2] = b;
}

// --- HTML Content (Embedded) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Matrix Viewport (v2)</title>
    <style>
        body { background: #1a1a1a; color: #eee; font-family: sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
        canvas { image-rendering: pixelated; border: 2px solid #444; box-shadow: 0 0 20px rgba(0,0,0,0.5); width: 320px; height: 320px; }
        #status { margin-top: 10px; font-size: 0.9em; color: #888; }
        #debug { margin-top: 5px; font-size: 0.8em; color: #555; font-family: monospace; }
    </style>
</head>
<body>
    <h1>Matrix Viewport</h1>
    <canvas id="matrix" width="32" height="32"></canvas>
    <div id="status">Connecting...</div>
    <div id="debug"></div>
    <script>
        const canvas = document.getElementById('matrix');
        const ctx = canvas.getContext('2d');
        const statusEl = document.getElementById('status');
        const debugEl = document.getElementById('debug');
        const width = 32; const height = 32;
        const imageData = ctx.createImageData(width, height);
        
        function connect() {
            // Determine WebSocket URL
            // If served from ESP32, use relative path. If local file, use hardcoded 192.168.4.1
            const proto = window.location.protocol === 'https:' ? 'wss' : 'ws';
            let host = window.location.hostname;
            if (!host) host = '192.168.4.1'; // Fallback for local files
            
            const wsUrl = `${proto}://${host}/ws`;
            
            debugEl.textContent = `Target: ${wsUrl}`;
            console.log('Connecting to', wsUrl);
            
            const ws = new WebSocket(wsUrl);
            ws.binaryType = 'arraybuffer';
            
            ws.onopen = () => { 
                statusEl.textContent = 'Connected (Live)'; 
                statusEl.style.color = '#4f4'; 
            };
            
            ws.onclose = () => { 
                statusEl.textContent = 'Disconnected. Retrying...'; 
                statusEl.style.color = '#f44'; 
                setTimeout(connect, 2000); 
            };
            
            ws.onerror = (e) => {
                debugEl.textContent += ' [Error]';
            };
            
            ws.onmessage = (event) => {
                if (event.data instanceof ArrayBuffer) {
                    const data = new Uint8Array(event.data);
                    if (data.length === width * height * 3) {
                        let ptr = 0;
                        for (let i = 0; i < width * height; i++) {
                            const canvasIdx = i * 4;
                            imageData.data[canvasIdx] = data[ptr++];
                            imageData.data[canvasIdx + 1] = data[ptr++];
                            imageData.data[canvasIdx + 2] = data[ptr++];
                            imageData.data[canvasIdx + 3] = 255;
                        }
                        ctx.putImageData(imageData, 0, 0);
                    }
                }
            };
        }
        
        // Initial connection
        connect();
    </script>
</body>
</html>
)rawliteral";

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("Websocket client connected");
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("Websocket client disconnected");
  }
}

void setup() {
  Serial.begin(115200);

  // 1. Init Matrix
  matrix.addLayer(&bg);
  matrix.setBrightness(120);
  matrix.begin();

  // 2. Init WiFi (AP Mode)
  WiFi.softAP(ssid, password);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // 3. Init Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
}

float idx = 0;

void loop() {
  // Generate simple plasma pattern
  bg.fillScreen({0, 0, 0});

  for (int y = 0; y < TOTAL_HEIGHT; y++) {
    for (int x = 0; x < TOTAL_WIDTH; x++) {
      float v = sin(x * 0.1 + idx) + cos(y * 0.1 + idx);
      uint8_t r = (sin(v * 3.14) + 1) * 127;
      uint8_t g = (cos(v * 2.0) + 1) * 127;
      uint8_t b = (sin(v * 1.0 + idx) + 1) * 127;

      drawPixelMirrored(x, y, r, g, b);
    }
  }

  bg.swapBuffers();

  // Retrieve framebuffer (not needed since we mirrored it during setPixel)

  // Broadcast framebuffer to all connected WebSocket clients
  // Only send text/binary if clients are connected to save resources
  if (ws.count() > 0) {
    ws.binaryAll(framebuffer, sizeof(framebuffer));
  }

  idx += 0.05;
  // Cap framerate slightly to not flood WebSocket
  delay(30);
}
