let SCREEN_X = 800;
let SCREEN_Y = 600;

let port;
let reader;
let writer;
let inputBuffer = "";
let isConnected = false;

// Calibration parameters
let targetDistance = 750; // cm (configurable)
let anchorIndex = 0; // which anchor to calibrate (configurable)
let currentAntennaDelay = 16336; // default value
let stepSize = 10;

// Data tracking
let rangeReadings = [];
let maxReadings = 20;
let averageRange = 0;
let errorPercentage = 0;

// UI elements
let distanceInput, anchorSelect, antennaDelayInput, stepSizeInput;
let connectBtn, setDelayBtn, saveBtn, restartBtn, clearBtn;
let statusDiv, readingsDiv, errorDiv;

function setup() {
  createCanvas(SCREEN_X, SCREEN_Y);
  
  createUI();
  
  // Initialize readings array
  rangeReadings = [];
}

function createUI() {
  // Title
  let title = createDiv('<h2>UWB Antenna Delay Calibration Tool</h2>');
  title.position(20, 10);
  
  // Connection section
  let connectionDiv = createDiv('<h3>Connection</h3>');
  connectionDiv.position(20, 50);
  
  connectBtn = createButton('Connect to Anchor');
  connectBtn.position(20, 100);
  connectBtn.mousePressed(connectSerial);
  
  statusDiv = createDiv('Status: Disconnected');
  statusDiv.position(160, 100);
  statusDiv.style('color', 'red');
  
  // Configuration section
  let configDiv = createDiv('<h3>Configuration</h3>');
  configDiv.position(20, 110);
  
  // Target distance input
  createDiv('Target Distance (cm):').position(20, 160);
  distanceInput = createInput(targetDistance.toString());
  distanceInput.position(160, 160);
  distanceInput.size(80);
  distanceInput.input(updateTargetDistance);
  
  // Anchor selection
  createDiv('Anchor Index (0-3):').position(20, 190);
  anchorSelect = createSelect();
  anchorSelect.position(160, 190);
  anchorSelect.size(80);
  for (let i = 0; i < 4; i++) {
    anchorSelect.option(i.toString(), i);
  }
  anchorSelect.changed(updateAnchorIndex);
  
  // Antenna delay input
  createDiv('Antenna Delay:').position(20, 220);
  antennaDelayInput = createInput(currentAntennaDelay.toString());
  antennaDelayInput.position(160, 220);
  antennaDelayInput.size(80);
  antennaDelayInput.input(updateAntennaDelay);
  
  // Step size input
  createDiv('Step Size:').position(20, 250);
  stepSizeInput = createInput(stepSize.toString());
  stepSizeInput.position(160, 250);
  stepSizeInput.size(80);
  stepSizeInput.input(updateStepSize);
  
  // Control buttons
  let controlDiv = createDiv('<h3>Controls</h3>');
  controlDiv.position(20, 270);
  
  let buttonY = 320;
  setDelayBtn = createButton('Set Antenna Delay');
  setDelayBtn.position(20, buttonY);
  setDelayBtn.mousePressed(setAntennaDelay);
  setDelayBtn.attribute('disabled', '');
  
  saveBtn = createButton('Save Config');
  saveBtn.position(150, buttonY);
  saveBtn.mousePressed(saveConfig);
  saveBtn.attribute('disabled', '');
  
  restartBtn = createButton('Restart Module');
  restartBtn.position(240, buttonY);
  restartBtn.mousePressed(restartModule);
  restartBtn.attribute('disabled', '');
  
  clearBtn = createButton('Clear Readings');
  clearBtn.position(20, buttonY + 30);
  clearBtn.mousePressed(clearReadings);
  
  // Quick adjustment buttons
  let quickDiv = createDiv('<h4>Quick Adjustments</h4>');
  quickDiv.position(20, 360);
  
  let quickButtonY = 410;
  let decreaseLargeBtn = createButton('-50');
  decreaseLargeBtn.position(20, quickButtonY);
  decreaseLargeBtn.mousePressed(() => adjustAntennaDelay(-50));
  
  let decreaseBtn = createButton('-10');
  decreaseBtn.position(70, quickButtonY);
  decreaseBtn.mousePressed(() => adjustAntennaDelay(-10));
  
  let increaseBtn = createButton('+10');
  increaseBtn.position(120, quickButtonY);
  increaseBtn.mousePressed(() => adjustAntennaDelay(10));
  
  let increaseLargeBtn = createButton('+50');
  increaseLargeBtn.position(170, quickButtonY);
  increaseLargeBtn.mousePressed(() => adjustAntennaDelay(50));
  
  // Readings display
  let readingsTitle = createDiv('<h3>Readings</h3>');
  readingsTitle.position(400, 110);
  
  readingsDiv = createDiv('No readings yet');
  readingsDiv.position(400, 160);
  readingsDiv.size(350, 200);
  readingsDiv.style('border', '1px solid #ccc');
  readingsDiv.style('padding', '10px');
  readingsDiv.style('background-color', '#f9f9f9');
  readingsDiv.style('overflow-y', 'scroll');
  
  errorDiv = createDiv('');
  errorDiv.position(400, 390);
  errorDiv.size(350, 50);
  errorDiv.style('font-weight', 'bold');
}

function updateTargetDistance() {
  targetDistance = parseInt(distanceInput.value()) || 750;
  calculateError();
}

function updateAnchorIndex() {
  anchorIndex = parseInt(anchorSelect.value());
  clearReadings();
}

function updateAntennaDelay() {
  currentAntennaDelay = parseInt(antennaDelayInput.value()) || 16336;
}

function updateStepSize() {
  stepSize = parseInt(stepSizeInput.value()) || 10;
}

async function connectSerial() {
  try {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    
    writer = port.writable.getWriter();
    
    // Send begin command
    const encoder = new TextEncoder();
    await writer.write(encoder.encode("begin\n"));
    
    isConnected = true;
    statusDiv.html('Status: Connected');
    statusDiv.style('color', 'green');
    
    connectBtn.html('Disconnect');
    setDelayBtn.removeAttribute('disabled');
    saveBtn.removeAttribute('disabled');
    restartBtn.removeAttribute('disabled');
    
    // Start reading
    readSerial();
    
  } catch (err) {
    console.error("Error connecting to serial port:", err);
    statusDiv.html('Status: Connection failed');
    statusDiv.style('color', 'red');
  }
}

async function readSerial() {
  if (!port) return;
  
  const decoder = new TextDecoder();
  reader = port.readable.getReader();
  
  try {
    while (isConnected) {
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
  
  const lines = inputBuffer.split("\n");
  inputBuffer = lines.pop() || "";
  
  for (const line of lines) {
    if (!line.trim()) continue;
    
    if (line.includes('AT+RANGE')) {
      const sections = line.split(',')
    }
    
    try {
      const data = JSON.parse(line);
      
      if (data.range && Array.isArray(data.range)) {
        const distance = data.range[anchorIndex];
        if (distance > 0) {
          addReading(distance);
        }
      }
    } catch (e) {
      // Log non-JSON messages
      console.log("[LOG] " + line);
      
      // Check for AT command responses
      if (line.includes("OK") || line.includes("ERROR")) {
        console.log("[AT Response] " + line);
      }
    }
  }
}

function addReading(distance) {
  rangeReadings.push({
    distance: distance,
    timestamp: millis(),
    antennaDelay: currentAntennaDelay
  });
  
  // Keep only the last maxReadings
  if (rangeReadings.length > maxReadings) {
    rangeReadings.shift();
  }
  
  calculateAverage();
  updateReadingsDisplay();
  calculateError();
}

function calculateAverage() {
  if (rangeReadings.length === 0) {
    averageRange = 0;
    return;
  }
  
  const sum = rangeReadings.reduce((acc, reading) => acc + reading.distance, 0);
  averageRange = sum / rangeReadings.length;
}

function calculateError() {
  if (averageRange === 0) {
    errorPercentage = 0;
    return;
  }
  
  errorPercentage = ((averageRange - targetDistance) / targetDistance) * 100;
}

function updateReadingsDisplay() {
  if (rangeReadings.length === 0) {
    readingsDiv.html('No readings yet');
    return;
  }
  
  let html = `<strong>Anchor ${anchorIndex} Readings:</strong><br>`;
  html += `Target Distance: ${targetDistance} cm<br>`;
  html += `Current Antenna Delay: ${currentAntennaDelay}<br>`;
  html += `Average Distance: ${averageRange.toFixed(1)} cm<br>`;
  html += `Error: ${errorPercentage.toFixed(2)}%<br><br>`;
  
  html += '<strong>Recent Readings:</strong><br>';
  const recentReadings = rangeReadings.slice(-10).reverse();
  
  for (const reading of recentReadings) {
    const error = ((reading.distance - targetDistance) / targetDistance) * 100;
    const color = Math.abs(error) < 5 ? 'green' : (Math.abs(error) < 10 ? 'orange' : 'red');
    html += `<span style="color: ${color}">${reading.distance} cm (${error.toFixed(1)}%)</span><br>`;
  }
  
  readingsDiv.html(html);
  
  // Update error display
  const errorColor = Math.abs(errorPercentage) < 5 ? 'green' : (Math.abs(errorPercentage) < 10 ? 'orange' : 'red');
  errorDiv.html(`<span style="color: ${errorColor}">Current Error: ${errorPercentage.toFixed(2)}%</span>`);
  
  if (Math.abs(errorPercentage) < 5) {
    errorDiv.html(errorDiv.html() + '<br><span style="color: green;">✓ Calibration within acceptable range!</span>');
  } else {
    const suggestion = errorPercentage > 0 ? 'decrease' : 'increase';
    const suggestedChange = Math.abs(errorPercentage) > 10 ? 50 : 10;
    errorDiv.html(errorDiv.html() + `<br><span style="color: blue;">Suggestion: ${suggestion} antenna delay by ~${suggestedChange}</span>`);
  }
}

async function sendATCommand(command) {
  if (!isConnected || !writer) {
    console.error("Not connected to serial port");
    return;
  }
  
  try {
    const encoder = new TextEncoder();
    await writer.write(encoder.encode(command + "\n"));
    console.log("Sent: " + command);
  } catch (error) {
    console.error("Error sending AT command:", error);
  }
}

async function setAntennaDelay() {
  const command = `AT+SETANT=${currentAntennaDelay}`;
  await sendATCommand(command);
  clearReadings(); // Clear readings after changing delay
}

async function saveConfig() {
  await sendATCommand("AT+SAVE");
}

async function restartModule() {
  await sendATCommand("AT+RESTART");
  clearReadings();
}

function clearReadings() {
  rangeReadings = [];
  averageRange = 0;
  errorPercentage = 0;
  updateReadingsDisplay();
}

function adjustAntennaDelay(adjustment) {
  currentAntennaDelay += adjustment;
  antennaDelayInput.value(currentAntennaDelay.toString());
  setAntennaDelay();
}

function draw() {
  background(240);
  
  // Draw a simple visualization
  fill('black');
  textSize(16);
  text("UWB Calibration Visualization", 20, SCREEN_Y - 100);
  
  if (rangeReadings.length > 0) {
    // Draw target distance line
    stroke('green');
    strokeWeight(3);
    const targetY = map(targetDistance, 0, 1000, SCREEN_Y - 20, SCREEN_Y - 80);
    line(20, targetY, SCREEN_X - 20, targetY);
    
    fill('green');
    noStroke();
    text(`Target: ${targetDistance}cm`, SCREEN_X - 150, targetY - 5);
    
    // Draw current average
    stroke('#FF00FF');
    strokeWeight(2);
    const avgY = map(averageRange, 0, 1000, SCREEN_Y - 20, SCREEN_Y - 80);
    line(20, avgY, SCREEN_X - 20, avgY);
    
    fill('#FF00FF');
    noStroke();
    text(`Average: ${averageRange.toFixed(1)}cm`, SCREEN_X - 150, avgY + 15);
    
    // Draw recent readings as points
    fill('black');
    noStroke();
    const recentReadings = rangeReadings.slice(-20);
    for (let i = 0; i < recentReadings.length; i++) {
      const x = map(i, 0, recentReadings.length - 1, 50, SCREEN_X - 50);
      const y = map(recentReadings[i].distance, 0, 1000, SCREEN_Y - 20, SCREEN_Y - 80);
      circle(x, y, 4);
    }
  }
}