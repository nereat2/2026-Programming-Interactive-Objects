/*
  Halftone Portrait Generator for 32x32 LED Matrix
  
  Instructions:
  1. Place a portrait image named "face.jpg" in the "data" folder inside this sketch folder.
  2. Run the sketch.
  
  Controls:
  - UP/DOWN: Adjust Contrast
  - LEFT/RIGHT: Adjust Gamma
  - 'D': Toggle Dithering (Bayer Matrix)
  - 'I': Invert Logic (Toggle between Bright=Large vs Dark=Large)
  - 'S': Save "output.png"
  
  The sketch stays strictly Black & White (Circles on Black Background).
*/

PImage img;
PImage sourceThumb;
float contrast = 1.0;
float gamma = 1.0;
boolean useDithering = true;
boolean invertLogic = false; // Default: Bright = Large Circle (Positive Image)

// Bayer Matrix 4x4 for ordered dithering
final int[][] bayer4x4 = {
  { 0,  8,  2, 10 },
  { 12, 4, 14,  6 },
  { 3, 11,  1,  9 },
  { 15, 7, 13,  5 }
};

void setup() {
  size(640, 640);
  noSmooth();
  ellipseMode(CENTER);
  
  // Load standard image
  img = loadImage("face.jpg");
  if (img == null) {
    println("Error: face.jpg not found in data folder!");
    // Create a dummy noise image if file missing
    img = createImage(512, 512, RGB);
    img.loadPixels();
    for(int i=0; i<img.pixels.length; i++) img.pixels[i] = color(random(255));
    img.updatePixels();
  }
  
  processImage();
}

void processImage() {
  // 1. Crop to Center Square
  int minDim = min(img.width, img.height);
  int cropX = (img.width - minDim) / 2;
  int cropY = (img.height - minDim) / 2;
  
  PImage cropped = img.get(cropX, cropY, minDim, minDim);
  
  // 2. Resize to 32x32
  // We use bilinear (smooth) scaling to preserve some detail before sampling
  cropped.resize(32, 32);
  sourceThumb = cropped;
}

void draw() {
  background(0);
  
  if (sourceThumb == null) return;
  sourceThumb.loadPixels();
  
  // Scale factor for display (32px -> 640px window)
  float scale = width / 32.0;
  float maxRadius = scale / 2.0; // Radius, so diameter is scale
  
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      
      // Get pixel brightness (0-255)
      int loc = x + y * 32;
      float b = brightness(sourceThumb.pixels[loc]);
      
      // Normalize 0-1
      float val = b / 255.0;
      
      // Apply Contrast
      // Simple contrast formula: factor * (val - 0.5) + 0.5
      val = (val - 0.5) * contrast + 0.5;
      val = constrain(val, 0.0, 1.0);
      
      // Apply Gamma
      if (gamma != 1.0) {
        val = pow(val, 1.0 / gamma);
      }
      
      // Logic Inversion check
      // User request: "darker = larger circle" -> Negative Image
      // LED Matrix standard: "brighter = larger circle" -> Positive Image
      // 'invertLogic' toggles this.
      float intensity = invertLogic ? (1.0 - val) : val;
      
      // Apply Dithering (Optional)
      if (useDithering) {
        // Map 0..1 to 0..16 range for comparison with Bayer
        float ditherVal = intensity * 17.0; // slight boost to reach pure white
        int bayerVal = bayer4x4[x % 4][y % 4];
        
        // Thresholding: If intensity is strong enough to pass the matrix threshold
        // But for radius-based halftone, dithering usually modulates the *radius*
        // or decides on/off. 
        // Let's use it to modulate effective brightness to break up banding.
        
        // Approach A: Dither adds noise to value before size calc
        float ditherNorm = (bayerVal / 16.0) - 0.5; // -0.5 to 0.5 center
        intensity += ditherNorm * 0.3; // 30% dither strength influence
        intensity = constrain(intensity, 0.0, 1.0);
      }
      
      // Draw Circle
      // Center position
      float cx = x * scale + scale/2;
      float cy = y * scale + scale/2;
      
      // Diameter
      // If intensity 0 -> Size 0
      // If intensity 1 -> Size 'scale' (Touching neighbors)
      float diam = intensity * scale * 0.95; // 0.95 to leave tiny gap
      
      if (diam > 0.5) {
        noStroke();
        fill(255);
        ellipse(cx, cy, diam, diam);
      }
    }
  }
  
  // Draw UI Text
  fill(0, 200);
  rect(0, height-40, width, 40);
  fill(0, 255, 0);
  textSize(14);
  textAlign(LEFT, CENTER);
  String mode = invertLogic ? "Dark=Large (Negative)" : "Bright=Large (Positive)";
  text(String.format("Contrast: %.2f | Gamma: %.2f | Dither: %s | Mode: %s", 
       contrast, gamma, useDithering ? "ON" : "OFF", mode), 10, height-20);
}

void keyPressed() {
  if (keyCode == UP) contrast += 0.1;
  if (keyCode == DOWN) contrast -= 0.1;
  if (keyCode == LEFT) gamma -= 0.1;
  if (keyCode == RIGHT) gamma += 0.1;
  
  if (key == 'd' || key == 'D') useDithering = !useDithering;
  if (key == 'i' || key == 'I') invertLogic = !invertLogic;
  
  // Safety clamps
  contrast = constrain(contrast, 0.0, 5.0);
  gamma = constrain(gamma, 0.1, 5.0);
  
  if (key == 's' || key == 'S') {
    saveFrame("output.png");
    println("Saved output.png");
  }
}
