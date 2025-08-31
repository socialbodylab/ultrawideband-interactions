/*
 * MaUWB_TAG.h - Header file for MaUWB-TAG class
 * 
 * Object-oriented UWB tag implementation with configurable parameters
 * and expandable anchor system.
 */

#ifndef MAUWB_TAG_H
#define MAUWB_TAG_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

class MaUWB_TAG {
private:
    // Configuration parameters
    uint8_t tagIndex;
    unsigned long refreshRate;
    unsigned long displayUpdateInterval;
    uint8_t maxTags;
    uint8_t positionHistoryLength;
    
    // Hardware components
    Adafruit_SSD1306* display;
    bool displayInitialized;
    
    // Anchor configuration
    static const uint8_t MAX_ANCHORS = 10;
    uint8_t numAnchors;
    float anchorX[MAX_ANCHORS];
    float anchorY[MAX_ANCHORS];
    
    // Distance measurements
    float distances[MAX_ANCHORS];
    
    // Position data
    float currentX;
    float currentY;
    
    // Position filtering
    static const uint8_t MAX_HISTORY = 10;
    float positionXHistory[MAX_HISTORY];
    float positionYHistory[MAX_HISTORY];
    uint8_t positionHistoryIndex;
    bool positionHistoryFilled;
      // Timing
    unsigned long lastDisplayUpdate;
    unsigned long lastRangeRequest;
    bool newData;
    
    // Debug control
    bool debugEnabled;
    
    // Serial communication
    String response;
    
    // Private methods
    void initializeHardware();
    void configureUWBModule();
    void parseRangeData(String data);
    void calculatePosition();
    bool calculatePositionFromTriplet(int a1, int a2, int a3, float& x, float& y);
    void updatePositionHistory(float x, float y);
    void updateDisplay();
    void displayAnchorDistance(int x, int y, int anchorNum, float distance);
    String sendCommand(String command, const int timeout, boolean debug = false);
    
public:
    // Constructor
    MaUWB_TAG(uint8_t tagIndex, unsigned long refreshRate = 50);
    
    // Destructor
    ~MaUWB_TAG();
    
    // Initialization
    bool begin();
    
    // Main update function - call this in loop()
    void update();
      // Configuration methods
    void setDisplayRefreshRate(unsigned long intervalMs);
    void setMaxTags(uint8_t maxTags);
    void setPositionHistoryLength(uint8_t length);
    
    // Debug control
    void enableDebug(bool enable = true);
    void disableDebug() { enableDebug(false); }
    bool isDebugEnabled() const;
      // Anchor management
    void setAnchorCount(uint8_t count);
    void setAnchorPosition(uint8_t anchorIndex, float x, float y);
    void setDefaultAnchors();
    
    // Individual anchor position setters (convenience methods)
    void anchor0(float x, float y) { setAnchorPosition(0, x, y); }
    void anchor1(float x, float y) { setAnchorPosition(1, x, y); }
    void anchor2(float x, float y) { setAnchorPosition(2, x, y); }
    void anchor3(float x, float y) { setAnchorPosition(3, x, y); }
    void anchor4(float x, float y) { setAnchorPosition(4, x, y); }
    void anchor5(float x, float y) { setAnchorPosition(5, x, y); }
    void anchor6(float x, float y) { setAnchorPosition(6, x, y); }
    void anchor7(float x, float y) { setAnchorPosition(7, x, y); }
    void anchor8(float x, float y) { setAnchorPosition(8, x, y); }
    void anchor9(float x, float y) { setAnchorPosition(9, x, y); }
    
    // Data access methods
    float getPositionX() const { return currentX; }
    float getPositionY() const { return currentY; }
    float getDistance(uint8_t anchorIndex) const;
    bool hasValidPosition() const;
    
    // Utility methods
    void requestRangeData();
    void processSerialData();
    void forwardSerialCommands();
      // Event callbacks (can be overridden by user)
    virtual void onPositionUpdate(float x, float y);
    virtual void onDistanceUpdate(uint8_t anchorIndex, float distance);
};

// Implementation

// Constructor
inline MaUWB_TAG::MaUWB_TAG(uint8_t tagIndex, unsigned long refreshRate) 
    : tagIndex(tagIndex), refreshRate(refreshRate), displayUpdateInterval(refreshRate),
      maxTags(8), positionHistoryLength(5), display(nullptr), displayInitialized(false),
      numAnchors(4), currentX(0), currentY(0), positionHistoryIndex(0), 
      positionHistoryFilled(false), lastDisplayUpdate(0), lastRangeRequest(0),
      newData(false), debugEnabled(false) {
    
    // Initialize arrays
    for (int i = 0; i < MAX_ANCHORS; i++) {
        distances[i] = 0.0;
        anchorX[i] = 0.0;
        anchorY[i] = 0.0;
    }
    
    for (int i = 0; i < MAX_HISTORY; i++) {
        positionXHistory[i] = 0.0;
        positionYHistory[i] = 0.0;
    }
    
    // Set default anchor positions
    setDefaultAnchors();
}

// Destructor
inline MaUWB_TAG::~MaUWB_TAG() {
    if (display) {
        delete display;
    }
}

// Initialize the UWB tag system
inline bool MaUWB_TAG::begin() {
    Serial.begin(115200);
    Serial.println("Starting MaUWB-TAG system...");
    
    initializeHardware();
    configureUWBModule();
    
    Serial.println("MaUWB-TAG initialized successfully");
    return true;
}

// Main update function
inline void MaUWB_TAG::update() {
    processSerialData();
    forwardSerialCommands();
    
    unsigned long currentTime = millis();
    
    // Request range data periodically
    if (currentTime - lastRangeRequest >= refreshRate) {
        requestRangeData();
        lastRangeRequest = currentTime;
    }
    
    // Update display if new data available and enough time has passed
    if (newData && (currentTime - lastDisplayUpdate >= displayUpdateInterval)) {
        updateDisplay();
        lastDisplayUpdate = currentTime;
        newData = false;
    }
}

// Initialize hardware components
inline void MaUWB_TAG::initializeHardware() {
    // Initialize OLED display
    display = new Adafruit_SSD1306(128, 64, &Wire, -1);
    
    if (display->begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        displayInitialized = true;
        display->clearDisplay();
        display->setTextSize(1);
        display->setTextColor(WHITE);
        display->setCursor(0, 0);
        display->println("MaUWB-TAG");
        display->println("Initializing...");
        display->display();
        
        if (debugEnabled) {
            Serial.println("OLED display initialized");
        }
    } else {
        displayInitialized = false;
        if (debugEnabled) {
            Serial.println("OLED display initialization failed");
        }
    }
}

// Configure UWB module
inline void MaUWB_TAG::configureUWBModule() {
    Serial.println("AT+anchor_tag=0");
    delay(100);
    
    String networkIdCommand = "AT+networkid=" + String(tagIndex);
    Serial.println(networkIdCommand);
    delay(100);
    
    if (debugEnabled) {
        Serial.println("UWB module configured as tag " + String(tagIndex));
    }
}

// Request range data from anchors
inline void MaUWB_TAG::requestRangeData() {
    String command = "AT+RANGE_CDS_ALL=" + String(tagIndex) + ",0,1,2,3";
    String response = sendCommand(command, 1000, debugEnabled);
    
    if (response.length() > 0) {
        parseRangeData(response);
    }
}

// Process incoming serial data
inline void MaUWB_TAG::processSerialData() {
    while (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (response.length() > 0) {
                parseRangeData(response);
                response = "";
            }
        } else {
            response += c;
        }
    }
}

// Forward serial commands from user
inline void MaUWB_TAG::forwardSerialCommands() {
    // This is handled in the main loop example
}

// Parse range data from UWB module
inline void MaUWB_TAG::parseRangeData(String data) {
    if (data.indexOf("+RANGE_CDS_ALL:") >= 0) {
        // Parse format: +RANGE_CDS_ALL:7,AN0,1.23,AN1,2.34,AN2,3.45,AN3,4.56
        int startIndex = data.indexOf(':') + 1;
        String rangeData = data.substring(startIndex);
        
        // Split by commas
        int commaIndex = 0;
        int lastComma = -1;
        int anchorIndex = 0;
        
        for (int i = 0; i < rangeData.length() && anchorIndex < numAnchors; i++) {
            if (rangeData.charAt(i) == ',' || i == rangeData.length() - 1) {
                commaIndex++;
                
                if (commaIndex % 2 == 0) { // Even comma indices have distance values
                    String distanceStr = rangeData.substring(lastComma + 1, i == rangeData.length() - 1 ? i + 1 : i);
                    float distance = distanceStr.toFloat();
                    
                    if (distance > 0 && anchorIndex < numAnchors) {
                        distances[anchorIndex] = distance;
                        onDistanceUpdate(anchorIndex, distance);
                        anchorIndex++;
                    }
                }
                lastComma = i;
            }
        }
        
        calculatePosition();
        newData = true;
        
        if (debugEnabled) {
            Serial.print("Distances: ");
            for (int i = 0; i < numAnchors; i++) {
                Serial.print("AN" + String(i) + ":" + String(distances[i]) + " ");
            }
            Serial.println();
        }
    }
}

// Calculate position using trilateration
inline void MaUWB_TAG::calculatePosition() {
    float newX = 0, newY = 0;
    bool positionFound = false;
    
    // Try different combinations of three anchors for trilateration
    for (int i = 0; i < numAnchors - 2 && !positionFound; i++) {
        for (int j = i + 1; j < numAnchors - 1 && !positionFound; j++) {
            for (int k = j + 1; k < numAnchors && !positionFound; k++) {
                if (distances[i] > 0 && distances[j] > 0 && distances[k] > 0) {
                    if (calculatePositionFromTriplet(i, j, k, newX, newY)) {
                        // Validate position is reasonable
                        if (newX >= -100 && newX <= 500 && newY >= -100 && newY <= 700) {
                            currentX = newX;
                            currentY = newY;
                            updatePositionHistory(newX, newY);
                            onPositionUpdate(newX, newY);
                            positionFound = true;
                        }
                    }
                }
            }
        }
    }
    
    if (debugEnabled && positionFound) {
        Serial.println("Position: (" + String(currentX) + ", " + String(currentY) + ")");
    }
}

// Calculate position from three anchor points using trilateration
inline bool MaUWB_TAG::calculatePositionFromTriplet(int a1, int a2, int a3, float& x, float& y) {
    float x1 = anchorX[a1], y1 = anchorY[a1], r1 = distances[a1];
    float x2 = anchorX[a2], y2 = anchorY[a2], r2 = distances[a2];
    float x3 = anchorX[a3], y3 = anchorY[a3], r3 = distances[a3];
    
    // Trilateration algorithm
    float A = 2 * (x2 - x1);
    float B = 2 * (y2 - y1);
    float C = pow(r1, 2) - pow(r2, 2) - pow(x1, 2) + pow(x2, 2) - pow(y1, 2) + pow(y2, 2);
    float D = 2 * (x3 - x2);
    float E = 2 * (y3 - y2);
    float F = pow(r2, 2) - pow(r3, 2) - pow(x2, 2) + pow(x3, 2) - pow(y2, 2) + pow(y3, 2);
    
    float denominator = A * E - B * D;
    
    if (abs(denominator) < 0.0001) {
        return false; // Points are collinear
    }
    
    x = (C * E - F * B) / denominator;
    y = (A * F - D * C) / denominator;
    
    return true;
}

// Update position history for filtering
inline void MaUWB_TAG::updatePositionHistory(float x, float y) {
    positionXHistory[positionHistoryIndex] = x;
    positionYHistory[positionHistoryIndex] = y;
    
    positionHistoryIndex = (positionHistoryIndex + 1) % positionHistoryLength;
    
    if (positionHistoryIndex == 0) {
        positionHistoryFilled = true;
    }
}

// Update OLED display
inline void MaUWB_TAG::updateDisplay() {
    if (!displayInitialized) return;
    
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(WHITE);
    display->setCursor(0, 0);
    
    display->println("MaUWB-TAG " + String(tagIndex));
    display->println("Position:");
    display->println("X: " + String(currentX, 1) + " cm");
    display->println("Y: " + String(currentY, 1) + " cm");
    
    // Display anchor distances
    for (int i = 0; i < min(numAnchors, 4); i++) {
        displayAnchorDistance(0, 35 + i * 7, i, distances[i]);
    }
    
    display->display();
}

// Display anchor distance on OLED
inline void MaUWB_TAG::displayAnchorDistance(int x, int y, int anchorNum, float distance) {
    display->setCursor(x, y);
    display->print("AN" + String(anchorNum) + ": " + String(distance, 1) + "m");
}

// Send command to UWB module and wait for response
inline String MaUWB_TAG::sendCommand(String command, const int timeout, boolean debug) {
    Serial.println(command);
    
    unsigned long start = millis();
    String response = "";
    
    while (millis() - start < timeout) {
        while (Serial.available()) {
            char c = Serial.read();
            response += c;
            
            if (response.endsWith("\n") || response.endsWith("\r\n")) {
                if (debug) {
                    Serial.print("Response: " + response);
                }
                return response;
            }
        }
    }
    
    if (debug) {
        Serial.println("Timeout waiting for response");
    }
    
    return "";
}

// Configuration methods
inline void MaUWB_TAG::setDisplayRefreshRate(unsigned long intervalMs) {
    displayUpdateInterval = intervalMs;
}

inline void MaUWB_TAG::setMaxTags(uint8_t maxTags) {
    this->maxTags = maxTags;
}

inline void MaUWB_TAG::setPositionHistoryLength(uint8_t length) {
    if (length <= MAX_HISTORY) {
        positionHistoryLength = length;
        positionHistoryIndex = 0;
        positionHistoryFilled = false;
    }
}

// Debug control methods
inline void MaUWB_TAG::enableDebug(bool enable) {
    debugEnabled = enable;
    Serial.println(enable ? "Debug enabled" : "Debug disabled");
}

inline bool MaUWB_TAG::isDebugEnabled() const {
    return debugEnabled;
}

// Anchor management methods
inline void MaUWB_TAG::setAnchorCount(uint8_t count) {
    if (count <= MAX_ANCHORS) {
        numAnchors = count;
    }
}

inline void MaUWB_TAG::setAnchorPosition(uint8_t anchorIndex, float x, float y) {
    if (anchorIndex < MAX_ANCHORS) {
        anchorX[anchorIndex] = x;
        anchorY[anchorIndex] = y;
        
        if (debugEnabled) {
            Serial.println("Anchor " + String(anchorIndex) + " set to (" + String(x) + ", " + String(y) + ")");
        }
    }
}

inline void MaUWB_TAG::setDefaultAnchors() {
    // Default rectangular layout
    setAnchorPosition(0, 0, 0);        // Top-left
    setAnchorPosition(1, 0, 600);      // Bottom-left  
    setAnchorPosition(2, 380, 600);    // Bottom-right
    setAnchorPosition(3, 380, 0);      // Top-right
}

// Data access methods
inline float MaUWB_TAG::getDistance(uint8_t anchorIndex) const {
    if (anchorIndex < MAX_ANCHORS) {
        return distances[anchorIndex];
    }
    return 0.0;
}

inline bool MaUWB_TAG::hasValidPosition() const {
    return (currentX != 0 || currentY != 0);
}

// Event callbacks (default implementations)
inline void MaUWB_TAG::onPositionUpdate(float x, float y) {
    // Default: do nothing, user can override
}

inline void MaUWB_TAG::onDistanceUpdate(uint8_t anchorIndex, float distance) {
    // Default: do nothing, user can override
}

#endif // MAUWB_TAG_H
