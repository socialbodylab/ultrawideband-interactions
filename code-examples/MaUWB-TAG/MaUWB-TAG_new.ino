/*
 * MaUWB-TAG - Object-Oriented UWB Tag Implementation
 * 
 * This is a modular, class-based implementation of UWB tag functionality
 * that provides distance measurement and position calculation using
 * multilateration algorithms.
 * 
 * Features:
 * - Object-oriented design for easy integration and reuse
 * - Configurable parameters (tag index, refresh rate, max tags)
 * - Expandable anchor system with dynamic positioning
 * - Real-time distance measurement from UWB anchors
 * - Advanced multilateration for 2D position calculation
 * - Position filtering and smoothing
 * - OLED display integration
 * - Configurable display update rates
 * 
 * Constructor Parameters:
 * - tagIndex: Unique identifier for this tag (0-255)
 * - refreshRate: Rate at which to request new distance data (ms)
 * 
 * Usage:
 *   MaUWB_TAG uwbTag(7, 50);  // Tag 7, 50ms refresh rate
 *   uwbTag.begin();
 *   // In loop:
 *   uwbTag.update();
 * 
 * Libraries needed:
 * - Wire (2.0.0)
 * - Adafruit_GFX_Library (1.11.7)
 * - Adafruit_BusIO (1.14.4)
 * - SPI (2.0.0)
 * - Adafruit_SSD1306 (2.5.7)
 */

// Uncomment to enable detailed debug logging
// #define DEBUG_MODE

// Include the MaUWB_TAG class header
#include "MaUWB_TAG.h"

// ESP32S3 pins
#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 39
#define I2C_SCL 38

#define SERIAL_LOG Serial
#define SERIAL_AT Serial2

// =============================================================================
// CLASS METHOD IMPLEMENTATIONS
// =============================================================================

// Constructor
MaUWB_TAG::MaUWB_TAG(uint8_t tagIndex, unsigned long refreshRate) {
    this->tagIndex = tagIndex;
    this->refreshRate = refreshRate;
    this->displayUpdateInterval = 50;  // Default 50ms display update
    this->maxTags = 10;  // Default max tags
    this->positionHistoryLength = 1;  // Default no averaging
    
    // Initialize anchor system with default 4 anchors
    this->numAnchors = 4;
    setDefaultAnchors();
    
    // Initialize distance array
    for (int i = 0; i < MAX_ANCHORS; i++) {
        distances[i] = 0.0;
    }
    
    // Initialize position data
    currentX = 0.0;
    currentY = 0.0;
    
    // Initialize history
    for (int i = 0; i < MAX_HISTORY; i++) {
        positionXHistory[i] = 0.0;
        positionYHistory[i] = 0.0;
    }
    positionHistoryIndex = 0;
    positionHistoryFilled = false;
    
    // Initialize timing
    lastDisplayUpdate = 0;
    lastRangeRequest = 0;
    newData = false;
    
    // Initialize debug (off by default)
    #ifdef DEBUG_MODE
    debugEnabled = true;
    #else
    debugEnabled = false;
    #endif
    
    // Initialize display
    display = new Adafruit_SSD1306(128, 64, &Wire, -1);
    displayInitialized = false;
    
    response.reserve(128);  // Pre-allocate string space
}

// Destructor
MaUWB_TAG::~MaUWB_TAG() {
    if (display) {
        delete display;
    }
}

// Initialize the UWB tag system
bool MaUWB_TAG::begin() {
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);

    SERIAL_LOG.begin(115200);
    delay(100);
    
    SERIAL_LOG.println(F("\n\n----- MaUWB-TAG INITIALIZATION -----"));
    SERIAL_LOG.print(F("Tag Index: "));
    SERIAL_LOG.println(tagIndex);
    SERIAL_LOG.print(F("Refresh Rate: "));
    SERIAL_LOG.print(refreshRate);
    SERIAL_LOG.println(F("ms"));
    
    // Initialize hardware
    initializeHardware();
    
    // Configure UWB module
    configureUWBModule();
    
    SERIAL_LOG.println(F("MaUWB-TAG initialization complete"));
    return true;
}

// Initialize hardware components
void MaUWB_TAG::initializeHardware() {
    // Initialize UWB module serial
    SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
    SERIAL_LOG.println(F("Serial2 initialized"));

    // Initialize I2C for display
    Wire.begin(I2C_SDA, I2C_SCL);
    SERIAL_LOG.println(F("I2C initialized"));
    
    delay(500);
    
    // Initialize OLED display
    SERIAL_LOG.println(F("Initializing SSD1306 display..."));
    if (!display->begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        SERIAL_LOG.println(F("ERROR: SSD1306 allocation failed"));
        return;
    }
    
    displayInitialized = true;
    SERIAL_LOG.println(F("SSD1306 display initialized successfully"));
    
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    
    // Show splash screen
    display->clearDisplay();
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->print(F("MaUWB-TAG "));
    display->println(tagIndex);
    display->setCursor(0, 15);
    display->println(F("Initializing..."));
    display->display();
}

// Configure the UWB module
void MaUWB_TAG::configureUWBModule() {
    SERIAL_LOG.println(F("Configuring UWB module as TAG..."));
    SERIAL_AT.println("AT");
    
    sendCommand("AT?", 500, true);
    sendCommand("AT+RESTORE", 1000, true);

    // Configure as tag (role=0)
    String cfg = "AT+SETCFG=" + String(tagIndex) + ",0,1,1";  // ID,role,freq,filter
    SERIAL_LOG.print(F("Config: "));
    SERIAL_LOG.println(cfg);
    sendCommand(cfg, 500, true);
    
    // Set capacity
    String cap = "AT+SETCAP=" + String(maxTags) + ",10,1";  // count,time,mode
    SERIAL_LOG.print(F("Capacity: "));
    SERIAL_LOG.println(cap);
    sendCommand(cap, 500, true);
    
    sendCommand("AT+SETRPT=1", 500, true);
    sendCommand("AT+SAVE", 500, true);
    sendCommand("AT+RESTART", 1000, true);
    
    // Show ready status
    if (displayInitialized) {
        display->clearDisplay();
        display->setCursor(0, 0);
        display->print(F("TAG "));
        display->println(tagIndex);
        display->setCursor(0, 15);
        display->println(F("Ready for data"));
        display->setCursor(0, 30);
        display->println(F("Waiting for measurements..."));
        display->display();
    }
    
    delay(1000);  // Give the UWB module time to initialize
}

// Main update function - call this in loop()
void MaUWB_TAG::update() {
    // Process incoming serial data
    processSerialData();
    
    // Forward commands from Serial Monitor
    forwardSerialCommands();
    
    // Update display on interval or when new data arrives
    if ((millis() - lastDisplayUpdate > displayUpdateInterval) || newData) {
        updateDisplay();
        newData = false;
        lastDisplayUpdate = millis();
    }
    
    // Request range data periodically
    unsigned long currentTime = millis();
    if (currentTime - lastRangeRequest > refreshRate) {
        requestRangeData();
        lastRangeRequest = currentTime;
    }
}

// Process incoming serial data from UWB module
void MaUWB_TAG::processSerialData() {
    while (SERIAL_AT.available() > 0) {
        char c = SERIAL_AT.read();

        if (c == '\r') {
            continue;
        } else if (c == '\n') {
            if (response.length() > 0) {
                SERIAL_LOG.print(F("UWB RESPONSE: "));
                SERIAL_LOG.println(response);
                
                parseRangeData(response);
                response = "";
            }
        } else {
            response += c;
        }
    }
}

// Forward commands from Serial Monitor to UWB module
void MaUWB_TAG::forwardSerialCommands() {
    while (SERIAL_LOG.available() > 0) {
        SERIAL_AT.write(SERIAL_LOG.read());
        yield();
    }
}

// Request range data from UWB module
void MaUWB_TAG::requestRangeData() {
    if (debugEnabled) {
        SERIAL_LOG.println(F("Requesting range data..."));
    }
    
    sendCommand("AT+RANGE", 100, false);
}

// Parse range information from UWB module response
void MaUWB_TAG::parseRangeData(String data) {
    int range_start = data.indexOf("range:(");
    if (range_start >= 0) {
        int range_end = data.indexOf(")", range_start);
        if (range_end > range_start) {
            int startIndex = range_start + 7;  // Skip "range:("
            int valueIndex = 0;
            
            if (debugEnabled) {
                SERIAL_LOG.print(F("Range values: "));
            }
            
            // Parse distance values
            while (valueIndex < numAnchors && startIndex < range_end) {
                int commaIndex = data.indexOf(',', startIndex);
                if (commaIndex == -1 || commaIndex > range_end) {
                    commaIndex = range_end;
                }
                
                char buffer[10];
                int len = commaIndex - startIndex;
                if (len < 10) {
                    strncpy(buffer, data.c_str() + startIndex, len);
                    buffer[len] = '\0';
                    distances[valueIndex] = atof(buffer);
                    
                    // Trigger distance update callback
                    onDistanceUpdate(valueIndex, distances[valueIndex]);
                }
                
                if (debugEnabled) {
                    SERIAL_LOG.print(distances[valueIndex]);
                    if (valueIndex < numAnchors - 1) SERIAL_LOG.print(F(", "));
                }
                
                valueIndex++;
                startIndex = commaIndex + 1;
            }
            
            if (debugEnabled) {
                SERIAL_LOG.println();
            }
            
            // Calculate position
            calculatePosition();
            newData = true;
        }
    }
}

// Calculate 2D position using multilateration
void MaUWB_TAG::calculatePosition() {
    // Need at least 3 anchors for 2D positioning
    if (numAnchors < 3) {
        SERIAL_LOG.println(F("Need at least 3 anchors for position calculation"));
        return;
    }
    
    // Check if we have valid distances
    int validDistances = 0;
    for (int i = 0; i < numAnchors; i++) {
        if (distances[i] > 0) validDistances++;
    }
    
    if (validDistances < 3) {
        SERIAL_LOG.println(F("Need valid distances from at least 3 anchors"));
        return;
    }
    
    // Use the same advanced multilateration algorithm as the original
    // Calculate multiple positions using different triplets and average them
    float sumX = 0, sumY = 0;
    int validSolutions = 0;
    
    // Try all possible triplet combinations
    for (int i = 0; i < numAnchors - 2; i++) {
        for (int j = i + 1; j < numAnchors - 1; j++) {
            for (int k = j + 1; k < numAnchors; k++) {
                if (distances[i] > 0 && distances[j] > 0 && distances[k] > 0) {
                    float x, y;
                    if (calculatePositionFromTriplet(i, j, k, x, y)) {
                        sumX += x;
                        sumY += y;
                        validSolutions++;
                    }
                }
            }
        }
    }
    
    if (validSolutions > 0) {
        float rawX = sumX / validSolutions;
        float rawY = sumY / validSolutions;
        
        // Boundary check
        if (rawX >= 0 && rawY >= 0 && rawX <= anchorX[2] && rawY <= anchorY[1]) {
            updatePositionHistory(rawX, rawY);
            
            if (debugEnabled) {
                SERIAL_LOG.print(F("Calculated position: X="));
                SERIAL_LOG.print(currentX);
                SERIAL_LOG.print(F(", Y="));
                SERIAL_LOG.println(currentY);
            }
            
            // Trigger position update callback
            onPositionUpdate(currentX, currentY);
        }
    }
}

// Calculate position from a triplet of anchors
bool MaUWB_TAG::calculatePositionFromTriplet(int a1, int a2, int a3, float& x, float& y) {
    // Linear system solving for 2D position
    float A1 = 2 * (anchorX[a2] - anchorX[a1]);
    float B1 = 2 * (anchorY[a2] - anchorY[a1]);
    float C1 = distances[a1]*distances[a1] - distances[a2]*distances[a2] - 
               anchorX[a1]*anchorX[a1] + anchorX[a2]*anchorX[a2] - 
               anchorY[a1]*anchorY[a1] + anchorY[a2]*anchorY[a2];
    
    float A2 = 2 * (anchorX[a3] - anchorX[a2]);
    float B2 = 2 * (anchorY[a3] - anchorY[a2]);
    float C2 = distances[a2]*distances[a2] - distances[a3]*distances[a3] - 
               anchorX[a2]*anchorX[a2] + anchorX[a3]*anchorX[a3] - 
               anchorY[a2]*anchorY[a2] + anchorY[a3]*anchorY[a3];
    
    float denominator = A1 * B2 - A2 * B1;
    if (abs(denominator) < 0.000001) {
        return false;  // No unique solution
    }
    
    x = (C1 * B2 - C2 * B1) / denominator;
    y = (A1 * C2 - A2 * C1) / denominator;
    
    return true;
}

// Update position history and calculate filtered position
void MaUWB_TAG::updatePositionHistory(float x, float y) {
    positionXHistory[positionHistoryIndex] = x;
    positionYHistory[positionHistoryIndex] = y;
    positionHistoryIndex = (positionHistoryIndex + 1) % positionHistoryLength;
    
    if (positionHistoryIndex == 0) {
        positionHistoryFilled = true;
    }
    
    // Calculate rolling average
    float avgX = 0, avgY = 0;
    int count = positionHistoryFilled ? positionHistoryLength : positionHistoryIndex;
    
    for (int i = 0; i < count; i++) {
        avgX += positionXHistory[i];
        avgY += positionYHistory[i];
    }
    
    if (count > 0) {
        currentX = avgX / count;
        currentY = avgY / count;
    }
}

// Update the display
void MaUWB_TAG::updateDisplay() {
    if (!displayInitialized) return;
    
    display->clearDisplay();

    // Header
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->print(F("TAG "));
    display->println(tagIndex);
    display->drawLine(0, 9, 128, 9, SSD1306_WHITE);
    
    // Display distances (show first 4 anchors)
    int maxDisplay = min(numAnchors, 4);
    for (int i = 0; i < maxDisplay; i++) {
        int x = (i % 2) * 64;
        int y = 12 + (i / 2) * 12;
        displayAnchorDistance(x, y, i, distances[i]);
    }
    
    // Divider
    display->drawLine(0, 35, 128, 35, SSD1306_WHITE);
    
    // Position
    display->setCursor(0, 40);
    display->println(F("POSITION:"));
    
    display->setCursor(0, 50);
    display->print(F("X: "));
    display->print(currentX, 1);
    
    display->setCursor(64, 50);
    display->print(F("Y: "));
    display->print(currentY, 1);
    
    display->display();
}

// Helper to display anchor distance
void MaUWB_TAG::displayAnchorDistance(int x, int y, int anchorNum, float distance) {
    display->setCursor(x, y);
    display->print(F("A"));
    display->print(anchorNum);
    display->print(F(": "));
    if (distance > 0) {
        display->print(distance, 1);
    } else {
        display->print(F("---"));
    }
}

// Send command to UWB module
String MaUWB_TAG::sendCommand(String command, const int timeout, boolean debug) {
    String response;
    response.reserve(64);

    if (debug) {
        SERIAL_LOG.print(F("CMD: "));
        SERIAL_LOG.println(command);
    }
    
    SERIAL_AT.println(command);
    unsigned long time = millis();

    while ((millis() - time) < timeout) {
        while (SERIAL_AT.available()) {
            char c = SERIAL_AT.read();
            response += c;
        }
    }

    if (debug && response.length() > 0) {
        SERIAL_LOG.print(F("RESP: "));
        SERIAL_LOG.println(response);
    }

    return response;
}

// Configuration methods
void MaUWB_TAG::setDisplayRefreshRate(unsigned long intervalMs) {
    displayUpdateInterval = intervalMs;
}

void MaUWB_TAG::setMaxTags(uint8_t maxTags) {
    this->maxTags = maxTags;
}

void MaUWB_TAG::setPositionHistoryLength(uint8_t length) {
    if (length > 0 && length <= MAX_HISTORY) {
        positionHistoryLength = length;
        positionHistoryIndex = 0;
        positionHistoryFilled = false;
    }
}

// Debug control methods
void MaUWB_TAG::enableDebug(bool enable) {
    debugEnabled = enable;
    if (debugEnabled) {
        SERIAL_LOG.println(F("Debug output enabled"));
    } else {
        SERIAL_LOG.println(F("Debug output disabled"));
    }
}

bool MaUWB_TAG::isDebugEnabled() const {
    return debugEnabled;
}

void MaUWB_TAG::setAnchorCount(uint8_t count) {
    if (count <= MAX_ANCHORS) {
        numAnchors = count;
    }
}

void MaUWB_TAG::setAnchorPosition(uint8_t anchorIndex, float x, float y) {
    if (anchorIndex < MAX_ANCHORS) {
        anchorX[anchorIndex] = x;
        anchorY[anchorIndex] = y;
    }
}

void MaUWB_TAG::setDefaultAnchors() {
    // Standard 4-anchor setup: rectangular boundary
    setAnchorPosition(0, 0, 0);      // Top-left
    setAnchorPosition(1, 0, 600);    // Bottom-left  
    setAnchorPosition(2, 380, 600);  // Bottom-right
    setAnchorPosition(3, 380, 0);    // Top-right
}

// Data access methods
float MaUWB_TAG::getDistance(uint8_t anchorIndex) const {
    if (anchorIndex < MAX_ANCHORS) {
        return distances[anchorIndex];
    }
    return -1.0;
}

bool MaUWB_TAG::hasValidPosition() const {
    return (currentX > 0 || currentY > 0);
}

// Event callbacks (can be overridden)
void MaUWB_TAG::onPositionUpdate(float x, float y) {
    // Default implementation - can be overridden by user
    // For example: trigger behaviors based on position zones
}

void MaUWB_TAG::onDistanceUpdate(uint8_t anchorIndex, float distance) {
    // Default implementation - can be overridden by user
    // For example: trigger proximity alerts
}

// =============================================================================
// MAIN PROGRAM
// =============================================================================

// Create UWB tag instance
MaUWB_TAG uwbTag(7, 50);  // Tag index 7, 50ms refresh rate

void setup() {
    // Configure anchor positions before initializing
    // Example: Custom anchor layout
    uwbTag.anchor0(0, 0);        // Top-left corner
    uwbTag.anchor1(0, 600);      // Bottom-left corner
    uwbTag.anchor2(380, 600);    // Bottom-right corner
    uwbTag.anchor3(380, 0);      // Top-right corner
    
    // Or use different positions:
    // uwbTag.anchor0(50, 50);
    // uwbTag.anchor1(50, 550);
    // uwbTag.anchor2(330, 550);
    // uwbTag.anchor3(330, 50);
    
    // Enable debug output (optional - disabled by default)
    // uwbTag.enableDebug();
    
    // Initialize the UWB tag system
    uwbTag.begin();
}

void loop() {
    // Main update - handles all UWB operations
    uwbTag.update();
    
    // Example: Toggle debug with serial commands
    // Send 'd' to enable debug, 'q' to disable debug
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'd' || cmd == 'D') {
            uwbTag.enableDebug(true);
        } else if (cmd == 'q' || cmd == 'Q') {
            uwbTag.disableDebug();
        }
    }
    
    // Example of accessing position data
    if (uwbTag.hasValidPosition()) {
        float x = uwbTag.getPositionX();
        float y = uwbTag.getPositionY();
        
        // Do something with position data
        // e.g., check if in specific zones, control outputs, etc.
    }
    
    // Example of accessing distance data
    for (int i = 0; i < 4; i++) {
        float dist = uwbTag.getDistance(i);
        if (dist > 0) {
            // Do something with distance to anchor i
        }
    }
}
