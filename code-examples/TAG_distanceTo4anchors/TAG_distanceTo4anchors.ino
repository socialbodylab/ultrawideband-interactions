/*
ESP32S3 UWB Tag - Distance Display
Displays distances to four anchors (0-3) according to the Makerfabs UWB AT Module format.

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
#define UWB_INDEX 0

// System configuration
#define UWB_TAG_COUNT 64
#define DISPLAY_UPDATE_INTERVAL 100  // Update display every 100ms

// ESP32S3 pins
#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 39
#define I2C_SCL 38

#define SERIAL_LOG Serial
#define SERIAL_AT Serial2

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Distance measurements to anchors (using anchors 0-3)
float dist_to_a0 = 0.0;
float dist_to_a1 = 0.0;
float dist_to_a2 = 0.0;
float dist_to_a3 = 0.0;

// For display updating
long last_display_update = 0;
bool new_data = false;
bool display_initialized = false;

void setup()
{
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);

    SERIAL_LOG.begin(115200);
    delay(100);
    
    SERIAL_LOG.println(F("\n\n----- ESP32S3 UWB TAG - DISTANCE DISPLAY -----"));
    
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
    display.println(F("UWB TAG - DISTANCES"));
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
    display.println(F("UWB TAG - DISTANCES"));
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
    
    // Request range data periodically (every 500ms)
    static long last_range_request = 0;
    if (millis() - last_range_request > 500) {
        SERIAL_LOG.println(F("Requesting range data..."));
        
        // Based on the AT command manual, we only need to send a single request
        // and the module will return distances to all anchors in one response
        sendData("AT+RANGE", 100, 1);  // Request all ranges at once
        
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
            
            new_data = true;
        }
    }
}

// Update the display with the latest distance measurements
void updateDistanceDisplay() {
    if (!display_initialized) {
        return;
    }
    
    display.clearDisplay();

    // Display header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("UWB TAG - DISTANCES"));
    display.drawLine(0, 9, 128, 9, SSD1306_WHITE);
    
    // Display distances with larger text for better visibility
    display.setCursor(0, 12);
    display.print(F("A0: "));
    
    // Show "---" if no measurement
    if (dist_to_a0 > 0) {
        display.print(dist_to_a0, 1);
        display.println(F(" cm"));
    } else {
        display.println(F("--- cm"));
    }
    
    display.setCursor(0, 24);
    display.print(F("A1: "));
    if (dist_to_a1 > 0) {
        display.print(dist_to_a1, 1);
        display.println(F(" cm"));
    } else {
        display.println(F("--- cm"));
    }
    
    display.setCursor(0, 36);
    display.print(F("A2: "));
    if (dist_to_a2 > 0) {
        display.print(dist_to_a2, 1);
        display.println(F(" cm"));
    } else {
        display.println(F("--- cm"));
    }
    
    display.setCursor(0, 48);
    display.print(F("A3: "));
    if (dist_to_a3 > 0) {
        display.print(dist_to_a3, 1);
        display.println(F(" cm"));
    } else {
        display.println(F("--- cm"));
    }
    
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