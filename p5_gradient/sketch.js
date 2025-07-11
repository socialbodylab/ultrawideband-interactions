const RED = [235, 52, 155];
const BLACK = [0, 0, 0];
const WHITE = [255, 255, 255];

let SCREEN_X = 800;
let SCREEN_Y = 900;

let anc = [];
let tag = [];
let anc_count = 4;
let tag_count = 8;

let A0X = 0,
  A0Y = 0;
let A1X = 0,
  A1Y = 1270;
let A2X = 540,
  A2Y = 1270;
let A3X = 540,
  A3Y = 0;

let ORIGIN_X, ORIGIN_Y;
let cm2p, x_offset, y_offset;

let port;
let reader;
let inputBuffer = "";
let lastUpdateTime = 0;
let frameRateValue = 0;
let dataReceived = 0;
let lastDataRateCalc = 0;
let dataRate = 0;
let ctx;
let gradientCanvas;

const tagColors = ['purple', 'green', 'purple', 'green', 'purple', '#FBA518', '#FBA518', '#FF00FF'];
const tagColorsGradient = [
  // [246, 186, 204], //t0
  // [255, 255, 255], //t1
  // [95, 132, 137], //t2
  // [95, 132, 137], //t3
  // [171, 172, 233], //t4
  // [253, 249, 252], //t5
  
  [196, 166, 230], //t0
  [247, 167, 210], //t1
  [152, 202, 245], //t2
  [174, 136, 250], //t3
  [255, 206, 167], //t4
  [254, 225, 236], //t5
  [0, 0, 0],
  [0, 0, 0]
];

class UWB {
  constructor(name, type, gradientColor) {
    this.name = name;
    this.type = type;
    this.x = 0;
    this.y = 0;
    this.status = false;
    this.list = Array(8).fill(0);
    this.needsUpdate = false;

    this.color = type === 1 ? RED : BLACK;
    this.gradientColor = gradientColor;
  }
  
  set_location(x, y) {
    this.x = x;
    this.y = y;
    this.status = true;
  }

  cal() {
    // Only run if update is needed
    if (!this.needsUpdate) return;
    this.needsUpdate = false;

    // Get distances to all 4 anchors
    const d = this.list;
    if (d.length < 4 || d[0] <= 0 || d[1] <= 0 || d[2] <= 0 || d[3] <= 0) {
      // Not enough valid distances
      return;
    }

    // Anchor positions
    const anchor_x = [anc[0].x, anc[1].x, anc[2].x, anc[3].x];
    const anchor_y = [anc[0].y, anc[1].y, anc[2].y, anc[3].y];

    // Precompute squared distances
    const dist_to_a0_sq = d[0] * d[0];
    const dist_to_a1_sq = d[1] * d[1];
    const dist_to_a2_sq = d[2] * d[2];
    const dist_to_a3_sq = d[3] * d[3];

    // Precompute anchor position terms
    const a0_sq = anchor_x[0] * anchor_x[0] + anchor_y[0] * anchor_y[0];
    const a1_sq = anchor_x[1] * anchor_x[1] + anchor_y[1] * anchor_y[1];
    const a2_sq = anchor_x[2] * anchor_x[2] + anchor_y[2] * anchor_y[2];
    const a3_sq = anchor_x[3] * anchor_x[3] + anchor_y[3] * anchor_y[3];

    // Common calculations for equations involving anchors
    const A_01 = 2 * (anchor_x[1] - anchor_x[0]);
    const B_01 = 2 * (anchor_y[1] - anchor_y[0]);
    const C_01 = dist_to_a0_sq - dist_to_a1_sq - a0_sq + a1_sq;

    const A_12 = 2 * (anchor_x[2] - anchor_x[1]);
    const B_12 = 2 * (anchor_y[2] - anchor_y[1]);
    const C_12 = dist_to_a1_sq - dist_to_a2_sq - a1_sq + a2_sq;

    const A_13 = 2 * (anchor_x[3] - anchor_x[1]);
    const B_13 = 2 * (anchor_y[3] - anchor_y[1]);
    const C_13 = dist_to_a1_sq - dist_to_a3_sq - a1_sq + a3_sq;

    const A_02 = 2 * (anchor_x[2] - anchor_x[0]);
    const B_02 = 2 * (anchor_y[2] - anchor_y[0]);
    const C_02 = dist_to_a0_sq - dist_to_a2_sq - a0_sq + a2_sq;

    const A_23 = 2 * (anchor_x[3] - anchor_x[2]);
    const B_23 = 2 * (anchor_y[3] - anchor_y[2]);
    const C_23 = dist_to_a2_sq - dist_to_a3_sq - a2_sq + a3_sq;

    // Solve using anchors 0,1,2
    let x1 = 0,
      y1 = 0,
      x2 = 0,
      y2 = 0,
      x3 = 0,
      y3 = 0;
    let validSolutions = 0;

    const den1 = A_01 * B_12 - B_01 * A_12;
    if (Math.abs(den1) > 0.000001) {
      x1 = (C_01 * B_12 - C_12 * B_01) / den1;
      y1 = (A_01 * C_12 - C_01 * A_12) / den1;
      validSolutions++;
    }

    // Solve using anchors 0,1,3
    const den2 = A_01 * B_13 - B_01 * A_13;
    if (Math.abs(den2) > 0.000001) {
      x2 = (C_01 * B_13 - C_13 * B_01) / den2;
      y2 = (A_01 * C_13 - C_01 * A_13) / den2;
      validSolutions++;
    }

    // Solve using anchors 0,2,3
    const den3 = A_02 * B_23 - B_02 * A_23;
    if (Math.abs(den3) > 0.000001) {
      x3 = (C_02 * B_23 - C_23 * B_02) / den3;
      y3 = (A_02 * C_23 - C_02 * A_23) / den3;
      validSolutions++;
    }

    if (validSolutions === 0) {
      // No valid solutions
      return;
    }

    // Average valid solutions
    let x = 0,
      y = 0;
    if (Math.abs(den1) > 0.000001) {
      x += x1;
      y += y1;
    }
    if (Math.abs(den2) > 0.000001) {
      x += x2;
      y += y2;
    }
    if (Math.abs(den3) > 0.000001) {
      x += x3;
      y += y3;
    }
    x /= validSolutions;
    y /= validSolutions;

    // Filter out negative or out-of-bounds values
    if (x < 0 || y < 0 || x > anchor_x[2] || y > anchor_y[1]) {
      // Out of bounds, skip update
      return;
    }

    // (Optional) You can implement a rolling average here if you want smoothing

    // Set the calculated position
    this.set_location(Math.floor(x), Math.floor(y));
  }

  three_point(x1, y1, x2, y2, r1, r2) {
    // Optimization: avoid unnecessary calculations
    if (r1 <= 0 || r2 <= 0) return [0, 0];

    // Calculate distance between centers more efficiently
    const dx = x2 - x1;
    const dy = y2 - y1;
    const p2p = Math.hypot(dx, dy); // More accurate and often faster than manual sqrt

    // Handle edge cases
    if (p2p < 0.001) return [x1, y1]; // Centers too close

    let temp_x, temp_y;

    // Check if circles intersect
    if (r1 + r2 <= p2p) {
      // No intersection - weighted position
      const ratio = r1 / (r1 + r2);
      temp_x = x1 + dx * ratio;
      temp_y = y1 + dy * ratio;
    } else {
      // Intersection case
      const dr = p2p / 2 + (r1 * r1 - r2 * r2) / (2 * p2p);
      const ratio = dr / p2p;
      temp_x = x1 + dx * ratio;
      temp_y = y1 + dy * ratio;
    }

    return [temp_x, temp_y];
  }
}

function setup() {
  createCanvas(SCREEN_X, SCREEN_Y);
  frameRate(120); // Set higher frame rate for smoother rendering

  // Create stats display
  const statsDiv = createDiv();
  statsDiv.position(180, SCREEN_Y);
  statsDiv.id("stats");

  // Initialize anchors and tags
  for (let i = 0; i < anc_count; i++) {
    anc.push(new UWB("ANC " + i, 0));
  }

  for (let i = 0; i < tag_count; i++) {
    tag.push(new UWB("TAG " + i, 1));
    tag[i].color = tagColors[i];
    tag[i].gradientColor = tagColorsGradient[i];
  }

  // Set anchor locations
  anc[0].set_location(A0X, A0Y);
  anc[1].set_location(A1X, A1Y);
  anc[2].set_location(A2X, A2Y);
  anc[3].set_location(A3X, A3Y);

  // Set origin and scaling
  ORIGIN_X = 0;
  ORIGIN_Y = 0;

  // Scale factor (3 cm = 1 pixel)
  cm2p = 0.6;
  x_offset = 200;
  y_offset = 20;

  // Set up connect button
  document
    .getElementById("connectButton")
    .addEventListener("click", connectSerial);
  
  document
  .getElementById("clearButton")
  .addEventListener("click", clearCanvas);

  lastUpdateTime = millis();
  lastDataRateCalc = millis();
  drawAnchorsBounds();
  
  ctx = drawingContext;
  gradientCanvas = createGraphics(width, height);
}

function clearCanvas() {
  drawAnchorsBounds();
}

async function connectSerial() {
  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });

    // Send begin command
    const encoder = new TextEncoder();
    const writer = port.writable.getWriter();
    await writer.write(encoder.encode("begin"));
    writer.releaseLock();

    // Start reading
    readSerial();
  } catch (err) {
    console.error("Error connecting to serial port:", err);
  }
}

async function readSerial() {
  if (!port) return;

  const decoder = new TextDecoder();
  reader = port.readable.getReader();

  try {
    // Use a more efficient approach with a larger buffer
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;

      processSerialData(decoder.decode(value));
    }
  } catch (error) {
    console.error("Error reading from serial:", error);
  } finally {
    if (reader) {
      reader.releaseLock();
    }
  }
}

function processSerialData(text) {
  inputBuffer += text;

  // Process all complete lines at once
  const lines = inputBuffer.split("\n");
  // Keep the last potentially incomplete line
  inputBuffer = lines.pop() || "";

  // Process each complete line
  for (const line of lines) {
    if (!line.trim()) continue;

    try {
      const data = JSON.parse(line);
      console.log(line);

      if (data.id !== undefined && data.range && data.id < tag_count) {
        tag[data.id].list = data.range;
        tag[data.id].needsUpdate = true;
        dataReceived++;
      }
    } catch (e) {
      console.log("[LOG]" + line);
    }
  }
}

function draw() {
  // Calculate FPS and data rate every second
  const now = millis();
  if (now - lastDataRateCalc > 1000) {
    frameRateValue = frameRate();
    dataRate = dataReceived;
    dataReceived = 0;
    lastDataRateCalc = now;

    // Update stats display
    select("#stats").html(
      `FPS: ${frameRateValue.toFixed(1)} | Data Rate: ${dataRate} packets/s`
    );
  }

  // Update all tags that need calculation
  for (let i = 0; i < tag.length; i++) {
    if (tag[i].needsUpdate) {
      tag[i].cal();
    }
  }

  // drawAnchorsBounds();
  background(0);
  
  // let maxRadius = Math.max(width, height);
  // gradientCanvas.clear();
  // gradientCanvas.drawingContext.globalCompositeOperation = 'screen';
  for (let i = 0; i < tag.length; i++) {
    const t = tag[i];
    drawUWB(t, true, 10);
  }
    
//     if(!t.status) continue;
//     let gradient = gradientCanvas.drawingContext.createRadialGradient(
//       t.x * cm2p + x_offset, t.y * cm2p + y_offset, 20,
//       t.x * cm2p + x_offset, t.y * cm2p + y_offset, 500
//     );
//     gradient.addColorStop(0, `rgba(${t.gradientColor[0]}, ${t.gradientColor[1]}, ${t.gradientColor[2]}, 1)`);
//     gradient.addColorStop(0.7, `rgba(${t.gradientColor[0]}, ${t.gradientColor[1]}, ${t.gradientColor[2]}, 0.4)`);
//     gradient.addColorStop(1, 'rgba(0, 0, 0, 0)');
    
//     gradientCanvas.drawingContext.fillStyle = gradient;
//     gradientCanvas.drawingContext.fillRect(0, 0, width, height);
//   }
//   gradientCanvas.drawingContext.globalCompositeOperation = 'source-over';
  
  drawGradientOptimized();
  image(gradientCanvas, 0, 0);
}

function drawAnchorsBounds() {
  background(WHITE);

  // Draw boundary rectangle
  stroke(BLACK);
  noFill();
  strokeWeight(4);
  rect(x_offset, y_offset, 540 * cm2p, 1270 * cm2p);

  // Draw anchors and tags
  for (let i = 0; i < anc.length; i++) {
    drawUWB(anc[i]);
  }

}

function drawUWB(uwb, drawLabel = true, radius = 20) {
  if (!uwb.status) return;

  // Calculate pixel coordinates - use integer coordinates for sharper rendering
  const pixel_x = int(uwb.x * cm2p + x_offset);
  const pixel_y = int(uwb.y * cm2p + y_offset);

  // Draw circle
  fill(uwb.color);
  noStroke();
  circle(pixel_x, pixel_y, radius);

  // Draw label
  if(drawLabel) {
    textSize(24);
    text(uwb.name + " (" + uwb.x + "," + uwb.y + ")", pixel_x + 10, pixel_y + 20);
  }
}

function drawGradientOptimized() {
  gradientCanvas.loadPixels();
  let pixels = gradientCanvas.pixels;
  const chunkSize = 4; // Process every 4th pixel, then interpolate
  
  // First pass: calculate gradient at reduced resolution
  for (let y = 0; y < height; y += chunkSize) {
    for (let x = 0; x < width; x += chunkSize) {
      let [r, g, b] = calculateBlendedColor(x, y);
      setPixelChunk(pixels, x, y, r, g, b, chunkSize);
    }
  }
  
  // Second pass: bilinear interpolation for smooth result
  interpolatePixels(pixels, chunkSize);
  
  gradientCanvas.updatePixels();
}

function calculateBlendedColor(x, y) {
  let totalR = 0, totalG = 0, totalB = 0, totalWeight = 0;
  
  for (let i = 0; i < tag.length; i++) {
    const t = tag[i];
    if(!t.status) continue; 
    let dx = x - (t.x * cm2p + x_offset);
    let dy = y - (t.y * cm2p + y_offset);
    let distSq = dx * dx + dy * dy + 1; // Add 1 to avoid division by zero
    let weight = 1 / (distSq * 0.3); // Inverse square falloff
    
    totalR += t.gradientColor[0] * weight;
    totalG += t.gradientColor[1] * weight;
    totalB += t.gradientColor[2] * weight;
    totalWeight += weight;
  }
  
  let r = totalR / totalWeight;
  let g = totalG / totalWeight;
  let b = totalB / totalWeight;
  
  // Simple saturation boost - push colors away from gray
  let gray = (r + g + b) / 3;
  let saturationBoost = 2; // Adjust this value (1.0 = no change, 2.0 = very saturated)
  
  r = gray + (r - gray) * saturationBoost;
  g = gray + (g - gray) * saturationBoost;
  b = gray + (b - gray) * saturationBoost;
  
  // return [
  //   Math.round(totalR / totalWeight),
  //   Math.round(totalG / totalWeight),
  //   Math.round(totalB / totalWeight)
  // ];
  
  return [
    Math.round(constrain(r, 0, 255)),
    Math.round(constrain(g, 0, 255)),
    Math.round(constrain(b, 0, 255))
  ];
}

function setPixelChunk(pixels, x, y, r, g, b, chunkSize) {
  for (let dy = 0; dy < chunkSize && y + dy < height; dy++) {
    for (let dx = 0; dx < chunkSize && x + dx < width; dx++) {
      let index = ((y + dy) * width + (x + dx)) * 4;
      pixels[index] = r;
      pixels[index + 1] = g;
      pixels[index + 2] = b;
      pixels[index + 3] = 255;
    }
  }
}

function interpolatePixels(pixels, chunkSize) {
  // Simple bilinear interpolation between chunk boundaries
  for (let y = chunkSize; y < height - chunkSize; y += chunkSize) {
    for (let x = chunkSize; x < width - chunkSize; x += chunkSize) {
      interpolateRegion(pixels, x - chunkSize, y - chunkSize, chunkSize);
    }
  }
}

function interpolateRegion(pixels, startX, startY, size) {
  // Get corner colors
  let tl = getPixelColor(pixels, startX, startY);
  let tr = getPixelColor(pixels, startX + size, startY);
  let bl = getPixelColor(pixels, startX, startY + size);
  let br = getPixelColor(pixels, startX + size, startY + size);
  
  // Interpolate intermediate pixels
  for (let y = 1; y < size; y++) {
    for (let x = 1; x < size; x++) {
      let fx = x / size;
      let fy = y / size;
      
      let top = lerpColor3(tl, tr, fx);
      let bottom = lerpColor3(bl, br, fx);
      let final = lerpColor3(top, bottom, fy);
      
      setPixelColor(pixels, startX + x, startY + y, final);
    }
  }
}

function getPixelColor(pixels, x, y) {
  let index = (y * width + x) * 4;
  return [pixels[index], pixels[index + 1], pixels[index + 2]];
}

function setPixelColor(pixels, x, y, color) {
  let index = (y * width + x) * 4;
  pixels[index] = color[0];
  pixels[index + 1] = color[1];
  pixels[index + 2] = color[2];
  pixels[index + 3] = 255;
}

function lerpColor3(c1, c2, t) {
  return [
    Math.round(c1[0] + (c2[0] - c1[0]) * t),
    Math.round(c1[1] + (c2[1] - c1[1]) * t),
    Math.round(c1[2] + (c2[2] - c1[2]) * t)
  ];
}