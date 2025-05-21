/*
ESP32S3 UWB Tag - Distance Display with Position Calculation
Displays distances to four anchors (0-3) and calculates 2D position.
Includes position filtering and smoothing.

Libraries needed:
- Wire (2.0.0)
- Adafruit_GFX_Library (1.11.7)
- Adafruit_BusIO (1.14.4)
- SPI (2.0.0)
- Adafruit_SSD1306 (2.5.7)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>





// Define tag ID
#define UWB_INDEX 7

// System configuration
#define UWB_TAG_COUNT 10
#define DISPLAY_UPDATE_INTERVAL 50  // Update display every 100ms

// Data request rate in milliseconds
unsigned long refreshRate = 50;  // Request range data every 500ms

// Position filtering configuration
#define POSITION_HISTORY_LENGTH 1  // Number of positions to average

// ESP32S3 pins
#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 39
#define I2C_SCL 38

#define SERIAL_LOG Serial
#define SERIAL_AT Serial2

Adafruit_SSD1306 display(128, 64, &Wire, -1);

//LED Pin
int LEDpin = 5;

//threshold
int yBuzzThreshold = 100;

// Distance measurements to anchors (using anchors 0-3)
float dist_to_a0 = 0.0;
float dist_to_a1 = 0.0;
float dist_to_a2 = 0.0;
float dist_to_a3 = 0.0;

// Anchor positions in cm
// A0 is top left (0,0)
// A1 is left bottom (0,600)
// A2 is right bottom (380,600)
// A3 is top right (380,0)
const float anchor_x[4] = {0, 0, 380, 380};
const float anchor_y[4] = {0, 600, 600, 0};

// Position of the tag
float positionX = 0.0;
float positionY = 0.0;

// Position history for rolling average
float positionX_history[POSITION_HISTORY_LENGTH] = {0};
float positionY_history[POSITION_HISTORY_LENGTH] = {0};
int position_history_index = 0;
bool position_history_filled = false;

// For display updating
long last_display_update = 0;
bool new_data = false;
bool display_initialized = false;

void setup()
{
    pinMode(LEDpin, OUTPUT);

    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);

    SERIAL_LOG.begin(115200);
    delay(100);
    
    SERIAL_LOG.println(F("\n\n----- ESP32S3 UWB TAG - DISTANCE & POSITION DISPLAY -----"));
    
    // Initialize UWB module serial
    SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
    SERIAL_LOG.println(F("Serial2 initialized"));

    // Initialize I2C for display
    Wire.begin(I2C_SDA, I2C_SCL);
    SERIAL_LOG.println(F("I2C initialized"));
    
    delay(500);
    
    // Initialize OLED display
    SERIAL_LOG.println(F("Initializing SSD1306 display..."));
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        SERIAL_LOG.println(F("ERROR: SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }
    
    display_initialized = true;
    SERIAL_LOG.println(F("SSD1306 display initialized successfully"));
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // Show splash screen
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("TAG "));
    display.println(UWB_INDEX);
    display.setCursor(0, 15);
    display.println(F("Initializing..."));
    display.display();
    
    // Configure the UWB module as a tag
    SERIAL_LOG.println(F("Configuring UWB module as TAG..."));
    SERIAL_AT.println("AT");
    
    sendData("AT?", 500, 1);
    sendData("AT+RESTORE", 1000, 1);

    // Configure as tag (role=0)
    String cfg = "AT+SETCFG=" + String(UWB_INDEX) + ",0,1,1";  // ID,role,freq,filter
    SERIAL_LOG.print(F("Config: "));
    SERIAL_LOG.println(cfg);
    sendData(cfg, 500, 1);
    
    // Set capacity
    String cap = "AT+SETCAP=" + String(UWB_TAG_COUNT) + ",10,1";  // count,time,mode
    SERIAL_LOG.print(F("Capacity: "));
    SERIAL_LOG.println(cap);
    sendData(cap, 500, 1);
    
    sendData("AT+SETRPT=1", 500, 1);
    sendData("AT+SAVE", 500, 1);
    sendData("AT+RESTART", 1000, 1);
    
    // Show display is ready
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("TAG "));
    display.println(UWB_INDEX);
    display.setCursor(0, 15);
    display.println(F("Ready for data"));
    display.setCursor(0, 30);
    display.println(F("Waiting for measurements..."));
    display.display();
    
    SERIAL_LOG.println(F("Setup complete - Reading distances to anchors"));
    delay(1000);  // Give the UWB module time to initialize
}

String response = "";

void loop()
{
    // Check for incoming data from UWB module
    while (SERIAL_AT.available() > 0)
    {
        char c = SERIAL_AT.read();

        if (c == '\r')
            continue;
        else if (c == '\n') {
            if (response.length() > 0) {
                SERIAL_LOG.print(F("UWB RESPONSE: "));
                SERIAL_LOG.println(response);
                
                // Parse any range information in the response
                parseRangeData(response);
                response = "";
            }
        }
        else {
            response += c;
        }
    }

    // Forward commands from Serial Monitor to UWB module
    while (SERIAL_LOG.available() > 0)
    {
        SERIAL_AT.write(SERIAL_LOG.read());
        yield();
    }
    
    // Update display on fixed interval or when new data arrives
    if ((millis() - last_display_update > DISPLAY_UPDATE_INTERVAL) || new_data) {
        updateDistanceDisplay();
        new_data = false;
        last_display_update = millis();
    }
    
    // Request range data periodically based on refreshRate
    static long last_range_request = 0;
    if (millis() - last_range_request > refreshRate) {
        SERIAL_LOG.println(F("Requesting range data..."));
        
        // Request all ranges at once
        sendData("AT+RANGE", 100, 1);
        
        last_range_request = millis();
    }
}

// Parse range information from UWB module
void parseRangeData(String data) {
    // According to the AT command manual, the response format is:
    // AT+RANGE=tid:x1,mask:x2,seq:x3,range:(x4,x5,x6,x7,x8,x9,x10,x11), rssi:(x12,x13,x14,x15,x16,x17,x18,x19)
    
    // Look for "range:(" in the response
    int range_start = data.indexOf("range:(");
    if (range_start >= 0) {
        // Find the end of the range array
        int range_end = data.indexOf(")", range_start);
        if (range_end > range_start) {
            // Extract just the range values
            String range_values = data.substring(range_start + 7, range_end);
            SERIAL_LOG.print(F("Range values: "));
            SERIAL_LOG.println(range_values);
            
            // Split the range values by commas
            int commaIndex = 0;
            int startIndex = 0;
            int valueIndex = 0;
            
            // We're only interested in the first 4 values (0-3)
            float distances[4] = {0, 0, 0, 0};
            
            // Parse each value
            while (valueIndex < 4 && startIndex < range_values.length()) {
                commaIndex = range_values.indexOf(',', startIndex);
                
                if (commaIndex == -1) {
                    // Last value
                    commaIndex = range_values.length();
                }
                
                String valueStr = range_values.substring(startIndex, commaIndex);
                distances[valueIndex] = valueStr.toFloat();
                
                SERIAL_LOG.print(F("Distance "));
                SERIAL_LOG.print(valueIndex);
                SERIAL_LOG.print(F(": "));
                SERIAL_LOG.println(distances[valueIndex]);
                
                valueIndex++;
                startIndex = commaIndex + 1;
            }
            
            // Update our distance variables
            dist_to_a0 = distances[0];
            dist_to_a1 = distances[1];
            dist_to_a2 = distances[2];
            dist_to_a3 = distances[3];
            
            // Calculate 2D position
            calculatePosition();
            
            // Respond to position data
            tagResponse(positionX, positionY);
            
            new_data = true;
        }
    }
}

// Calculate 2D position using simple triangulation
void calculatePosition() {
    // Check if we have valid distances from all 4 anchors
    if (dist_to_a0 <= 0 || dist_to_a1 <= 0 || dist_to_a2 <= 0 || dist_to_a3 <= 0) {
        SERIAL_LOG.println(F("Cannot calculate position - need distances from all 4 anchors"));
        return;
    }
    
    // Using simple triangulation with three anchors (A0, A1, A2)
    // This method uses the first three anchors for a simple triangulation calculation
    
    // First, convert distances from cm to m to improve numerical stability
    float r1 = dist_to_a0 / 100.0;  // A0 distance in meters
    float r2 = dist_to_a1 / 100.0;  // A1 distance in meters
    float r3 = dist_to_a2 / 100.0;  // A2 distance in meters
    
    // Convert anchor positions to meters
    float x1 = anchor_x[0] / 100.0;  // A0 x in meters
    float y1 = anchor_y[0] / 100.0;  // A0 y in meters
    float x2 = anchor_x[1] / 100.0;  // A1 x in meters
    float y2 = anchor_y[1] / 100.0;  // A1 y in meters
    float x3 = anchor_x[2] / 100.0;  // A2 x in meters
    float y3 = anchor_y[2] / 100.0;  // A2 y in meters
    
    // Calculate intermediate values for triangulation
    float A = 2 * (x2 - x1);
    float B = 2 * (y2 - y1);
    float C = r1 * r1 - r2 * r2 - x1 * x1 + x2 * x2 - y1 * y1 + y2 * y2;
    float D = 2 * (x3 - x2);
    float E = 2 * (y3 - y2);
    float F = r2 * r2 - r3 * r3 - x2 * x2 + x3 * x3 - y2 * y2 + y3 * y3;
    
    // Calculate position (in meters)
    float x = 0.0, y = 0.0;
    
    // Check if we can solve the system
    float denominator = (A * E - B * D);
    if (abs(denominator) < 0.000001) {
        SERIAL_LOG.println(F("Cannot calculate position - anchors are colinear"));
        return;
    }
    
    // Solve for position
    x = (C * E - F * B) / denominator;
    y = (A * F - C * D) / denominator;
    
    // Convert back to cm and store the raw calculated position
    float rawX = x * 100.0;
    float rawY = y * 100.0;
    
    // Filter out negative values or values outside the boundary
    if (rawX < 0 || rawY < 0 || rawX > anchor_x[2] || rawY > anchor_y[1]) {
        SERIAL_LOG.println(F("Position outside valid boundaries - skipping update"));
        return;
    }
    
    // Add the new position to our history
    positionX_history[position_history_index] = rawX;
    positionY_history[position_history_index] = rawY;
    position_history_index = (position_history_index + 1) % POSITION_HISTORY_LENGTH;
    
    if (position_history_index == 0) {
        position_history_filled = true;
    }
    
    // Calculate the rolling average
    float avgX = 0, avgY = 0;
    int count = position_history_filled ? POSITION_HISTORY_LENGTH : position_history_index;
    
    for (int i = 0; i < count; i++) {
        avgX += positionX_history[i];
        avgY += positionY_history[i];
    }
    
    if (count > 0) {
        avgX /= count;
        avgY /= count;
    }
    
    // Update the final filtered position
    positionX = avgX;
    positionY = avgY;
    
    SERIAL_LOG.print(F("Raw position: X="));
    SERIAL_LOG.print(rawX);
    SERIAL_LOG.print(F(", Y="));
    SERIAL_LOG.println(rawY);
    
    SERIAL_LOG.print(F("Filtered position: X="));
    SERIAL_LOG.print(positionX);
    SERIAL_LOG.print(F(", Y="));
    SERIAL_LOG.println(positionY);
}

// Function for tag to respond to its position
void tagResponse(float x, float y) {
    // do something with the position data
    
    // For example, this could:
    // - Trigger different behaviors in different zones
    // - Control LEDs or sounds based on position
    // - Send position data to other devices
    // - Detect proximity to points of interest

    if(y>=yBuzzThreshold)
    {
        digitalWrite(LEDpin, HIGH);
    }
    else
    {
        digitalWrite(LEDpin, LOW);
    }



}

// Update the display with the latest distance measurements and position
void updateDistanceDisplay() {
    if (!display_initialized) {
        return;
    }
    
    display.clearDisplay();

    // Display header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("TAG "));
    display.println(UWB_INDEX);
    display.drawLine(0, 9, 128, 9, SSD1306_WHITE);
    
    // Display distance to A0 and A1
    display.setCursor(0, 12);
    display.print(F("A0: "));
    if (dist_to_a0 > 0) {
        display.print(dist_to_a0, 1);
    } else {
        display.print(F("---"));
    }
    
    display.setCursor(64, 12);
    display.print(F("A1: "));
    if (dist_to_a1 > 0) {
        display.print(dist_to_a1, 1);
    } else {
        display.print(F("---"));
    }
    
    // Display distance to A2 and A3
    display.setCursor(0, 24);
    display.print(F("A2: "));
    if (dist_to_a2 > 0) {
        display.print(dist_to_a2, 1);
    } else {
        display.print(F("---"));
    }
    
    display.setCursor(64, 24);
    display.print(F("A3: "));
    if (dist_to_a3 > 0) {
        display.print(dist_to_a3, 1);
    } else {
        display.print(F("---"));
    }
    
    // Display divider
    display.drawLine(0, 35, 128, 35, SSD1306_WHITE);
    
    // Display position
    display.setCursor(0, 40);
    display.println(F("POSITION:"));
    
    display.setCursor(0, 50);
    display.print(F("X: "));
    display.print(positionX, 1);
    
    display.setCursor(64, 50);
    display.print(F("Y: "));
    display.print(positionY, 1);
    
    display.display();
}

// Send command to the UWB module and get response
String sendData(String command, const int timeout, boolean debug)
{
    String response = "";

    if (debug) {
        SERIAL_LOG.print(F("CMD: "));
        SERIAL_LOG.println(command);
    }
    
    SERIAL_AT.println(command);

    long int time = millis();

    while ((time + timeout) > millis())
    {
        while (SERIAL_AT.available())
        {
            char c = SERIAL_AT.read();
            response += c;
        }
    }

    if (debug && response.length() > 0)
    {
        SERIAL_LOG.print(F("RESP: "));
        SERIAL_LOG.println(response);
    }

    return response;
}