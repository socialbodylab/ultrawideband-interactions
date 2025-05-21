/*
ESP32S3 UWB Tag - AT Command Response Simulator
This sketch simulates AT command responses from a UWB module
for testing the position calculation algorithm without physical hardware.

PURPOSE:
This file provides a basic AT command simulator for testing the position calculation 
algorithm with synthetic data. It helps validate the multilateration algorithm by 
creating realistic UWB module responses.

FEATURES:
- Tests predefined positions across the room (center, corners, etc.)
- Adds variable amounts of noise to distance measurements
- Generates AT command responses in the same format as the UWB module
- Parses responses using the same algorithm as the main code
- Calculates and reports position errors

USAGE:
- Run this sketch on an ESP32 and open serial monitor
- Type 'help' for available commands
- 'test' runs all test points without noise
- 'test noise=X' runs tests with X% noise (e.g., 'test noise=5')
- 'position X,Y' tests a specific position
- 'at' generates an AT command for current distances

The anchor positions match the configuration in the main program:
- A0: (0,0)     - top left
- A1: (0,600)   - bottom left
- A2: (380,600) - bottom right
- A3: (380,0)   - top right

All distances are in centimeters.
*/

// Uncomment to enable detailed debug logging
#define DEBUG_MODE

#include <Arduino.h>

// Define AT command response format
#define AT_RESP_PREFIX "AT+RANGE=tid:1,mask:0x0F,seq:0,range:("
#define AT_RESP_SUFFIX "),rssi:(-77,-78,-76,-77,0,0,0,0)"

// Anchor positions in cm
// A0 is top left (0,0)
// A1 is left bottom (0,600)
// A2 is right bottom (380,600)
// A3 is top right (380,0)
const float anchor_x[4] = {0, 0, 380, 380};
const float anchor_y[4] = {0, 600, 600, 0};

// Test points to simulate tag positions
#define NUM_TEST_POINTS 5
const float test_points[NUM_TEST_POINTS][2] = {
    {190, 300},  // Center of the rectangle
    {100, 100},  // Near top left
    {290, 500},  // Near bottom right
    {50, 500},   // Near bottom left
    {300, 50}    // Near top right
};

// Distance measurements to anchors
float dist_to_a0 = 0.0;
float dist_to_a1 = 0.0;
float dist_to_a2 = 0.0;
float dist_to_a3 = 0.0;

// Position of the tag
float positionX = 0.0;
float positionY = 0.0;

// For simulating position with noise
bool add_noise = false;
float noise_percentage = 0.0;  // Default no noise

// Calculate distance from (x1,y1) to (x2,y2)
float calculateDistance(float x1, float y1, float x2, float y2) {
    return sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
}

// Generate AT command response string with distances
String generateATResponse(float d0, float d1, float d2, float d3) {
    String resp = AT_RESP_PREFIX;
    
    resp += String(d0, 1);  // 1 decimal place for distance
    resp += ",";
    resp += String(d1, 1);
    resp += ",";
    resp += String(d2, 1);
    resp += ",";
    resp += String(d3, 1);
    resp += ",0,0,0,0";  // Padding for unused anchors
    
    resp += AT_RESP_SUFFIX;
    
    return resp;
}

// Calculate 2D position using enhanced multilateration with all 4 anchors
void calculatePosition() {
    // Check if we have valid distances from all 4 anchors
    if (dist_to_a0 <= 0 || dist_to_a1 <= 0 || dist_to_a2 <= 0 || dist_to_a3 <= 0) {
        Serial.println("Cannot calculate position - need distances from all 4 anchors");
        return;
    }
    
    // Work directly in cm to avoid unnecessary conversions
    // This is a non-iterative multilateration approach using all 4 anchors
    
    // We'll calculate 3 separate positions using different triplets of anchors,
    // then take the weighted average
    float x1 = 0, y1 = 0;  // Position calculated from anchors 0,1,2
    float x2 = 0, y2 = 0;  // Position calculated from anchors 0,1,3
    float x3 = 0, y3 = 0;  // Position calculated from anchors 0,2,3
    
    // Precompute squared distances and other repeated values
    float dist_to_a0_sq = dist_to_a0 * dist_to_a0;
    float dist_to_a1_sq = dist_to_a1 * dist_to_a1;
    float dist_to_a2_sq = dist_to_a2 * dist_to_a2;
    float dist_to_a3_sq = dist_to_a3 * dist_to_a3;
    
    // Precompute anchor position terms
    float a0_sq = anchor_x[0] * anchor_x[0] + anchor_y[0] * anchor_y[0];
    float a1_sq = anchor_x[1] * anchor_x[1] + anchor_y[1] * anchor_y[1];
    float a2_sq = anchor_x[2] * anchor_x[2] + anchor_y[2] * anchor_y[2];
    float a3_sq = anchor_x[3] * anchor_x[3] + anchor_y[3] * anchor_y[3];
    
    // Common calculations for equations involving anchors 0-1
    float A_01 = 2 * (anchor_x[1] - anchor_x[0]);
    float B_01 = 2 * (anchor_y[1] - anchor_y[0]);
    float C_01 = dist_to_a0_sq - dist_to_a1_sq - a0_sq + a1_sq;
    
    // Common calculations for other anchor pairs
    float A_12 = 2 * (anchor_x[2] - anchor_x[1]);
    float B_12 = 2 * (anchor_y[2] - anchor_y[1]);
    float C_12 = dist_to_a1_sq - dist_to_a2_sq - a1_sq + a2_sq;
    
    float A_13 = 2 * (anchor_x[3] - anchor_x[1]);
    float B_13 = 2 * (anchor_y[3] - anchor_y[1]);
    float C_13 = dist_to_a1_sq - dist_to_a3_sq - a1_sq + a3_sq;
    
    float A_02 = 2 * (anchor_x[2] - anchor_x[0]);
    float B_02 = 2 * (anchor_y[2] - anchor_y[0]);
    float C_02 = dist_to_a0_sq - dist_to_a2_sq - a0_sq + a2_sq;
    
    float A_23 = 2 * (anchor_x[3] - anchor_x[2]);
    float B_23 = 2 * (anchor_y[3] - anchor_y[2]);
    float C_23 = dist_to_a2_sq - dist_to_a3_sq - a2_sq + a3_sq;
    
    // Solve using anchors 0,1,2
    float den1 = (A_01 * B_12 - B_01 * A_12);
    if (abs(den1) > 0.000001) {
        x1 = (C_01 * B_12 - C_12 * B_01) / den1;
        y1 = (A_01 * C_12 - C_01 * A_12) / den1;
        
        Serial.print("Position from anchors 0,1,2: (");
        Serial.print(x1);
        Serial.print(", ");
        Serial.print(y1);
        Serial.println(")");
    }
    
    // Solve using anchors 0,1,3
    float den2 = (A_01 * B_13 - B_01 * A_13);
    if (abs(den2) > 0.000001) {
        x2 = (C_01 * B_13 - C_13 * B_01) / den2;
        y2 = (A_01 * C_13 - C_01 * A_13) / den2;
        
        Serial.print("Position from anchors 0,1,3: (");
        Serial.print(x2);
        Serial.print(", ");
        Serial.print(y2);
        Serial.println(")");
    }
    
    // Solve using anchors 0,2,3
    float den3 = (A_02 * B_23 - B_02 * A_23);
    if (abs(den3) > 0.000001) {
        x3 = (C_02 * B_23 - C_23 * B_02) / den3;
        y3 = (A_02 * C_23 - C_02 * A_23) / den3;
        
        Serial.print("Position from anchors 0,2,3: (");
        Serial.print(x3);
        Serial.print(", ");
        Serial.print(y3);
        Serial.println(")");
    }
    
    // Count valid solutions
    int validSolutions = 0;
    if (abs(den1) > 0.000001) validSolutions++;
    if (abs(den2) > 0.000001) validSolutions++;
    if (abs(den3) > 0.000001) validSolutions++;
    
    if (validSolutions == 0) {
        Serial.println("Cannot calculate position - no valid solutions");
        return;
    }
    
    // Calculate position as average of valid solutions
    float x = 0.0, y = 0.0;
    
    if (abs(den1) > 0.000001) {
        x += x1;
        y += y1;
    }
    if (abs(den2) > 0.000001) {
        x += x2;
        y += y2;
    }
    if (abs(den3) > 0.000001) {
        x += x3;
        y += y3;
    }
    
    x /= validSolutions;
    y /= validSolutions;
    
    // Store the calculated position
    positionX = x;
    positionY = y;
    
    Serial.print("Final position (average of ");
    Serial.print(validSolutions);
    Serial.print(" solutions): (");
    Serial.print(positionX);
    Serial.print(", ");
    Serial.print(positionY);
    Serial.println(")");
}

// Function to apply noise to distances
void applyNoise() {
    if (!add_noise || noise_percentage <= 0) return;
    
    // Apply random noise within the noise percentage range
    float noise_factor1 = 1.0 + random(-100, 100) * noise_percentage / 100.0;
    float noise_factor2 = 1.0 + random(-100, 100) * noise_percentage / 100.0;
    float noise_factor3 = 1.0 + random(-100, 100) * noise_percentage / 100.0;
    float noise_factor4 = 1.0 + random(-100, 100) * noise_percentage / 100.0;
    
    dist_to_a0 *= noise_factor1;
    dist_to_a1 *= noise_factor2;
    dist_to_a2 *= noise_factor3;
    dist_to_a3 *= noise_factor4;
    
    Serial.print("Applied noise: ");
    Serial.print((noise_factor1 - 1.0) * 100.0, 1);
    Serial.print("%, ");
    Serial.print((noise_factor2 - 1.0) * 100.0, 1);
    Serial.print("%, ");
    Serial.print((noise_factor3 - 1.0) * 100.0, 1);
    Serial.print("%, ");
    Serial.print((noise_factor4 - 1.0) * 100.0, 1);
    Serial.println("%");
}

// Parse range information from AT command response
void parseRangeData(String data) {
    // According to the AT command manual, the response format is:
    // AT+RANGE=tid:x1,mask:x2,seq:x3,range:(x4,x5,x6,x7,x8,x9,x10,x11), rssi:(x12,x13,x14,x15,x16,x17,x18,x19)
    
    // Look for "range:(" in the response
    int range_start = data.indexOf("range:(");
    if (range_start >= 0) {
        // Find the end of the range array
        int range_end = data.indexOf(")", range_start);
        if (range_end > range_start) {
            // We're only interested in the first 4 values (0-3)
            float distances[4] = {0, 0, 0, 0};
            
            // Parse directly from the original string without creating substrings
            int startIndex = range_start + 7;  // Skip "range:("
            int valueIndex = 0;
            
            Serial.print("Range values: ");
            
            // Parse each value directly
            while (valueIndex < 4 && startIndex < range_end) {
                // Find the next comma or the end parenthesis
                int commaIndex = data.indexOf(',', startIndex);
                if (commaIndex == -1 || commaIndex > range_end) {
                    commaIndex = range_end;
                }
                
                // Convert this segment directly to float
                char buffer[10]; // Temporary buffer for number conversion
                int len = commaIndex - startIndex;
                if (len < 10) {
                    strncpy(buffer, data.c_str() + startIndex, len);
                    buffer[len] = '\0';
                    distances[valueIndex] = atof(buffer);
                }
                
                Serial.print(distances[valueIndex]);
                if (valueIndex < 3) Serial.print(", ");
                
                valueIndex++;
                startIndex = commaIndex + 1;
            }
            
            Serial.println();
            
            // Update our distance variables
            dist_to_a0 = distances[0];
            dist_to_a1 = distances[1];
            dist_to_a2 = distances[2];
            dist_to_a3 = distances[3];
            
            // Calculate 2D position
            calculatePosition();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    randomSeed(analogRead(0));  // Initialize random seed for noise generation
    
    Serial.println("\n\n----- AT COMMAND SIMULATOR FOR UWB POSITION TESTING -----");
    Serial.println("This sketch simulates AT command responses for testing the position calculation algorithm.");
    Serial.println("Type 'help' for available commands.");
}

void processSerialCommand() {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "help") {
        Serial.println("\n----- Available Commands -----");
        Serial.println("test              - Run all test points without noise");
        Serial.println("test noise=X      - Run all test points with X% noise (e.g., noise=5)");
        Serial.println("position X,Y      - Test specific position (e.g., position 150,200)");
        Serial.println("at                - Generate AT command response for current position");
        Serial.println("----------------");
    }
    else if (cmd.startsWith("test")) {
        // Check if noise parameter is specified
        int noisePos = cmd.indexOf("noise=");
        if (noisePos >= 0) {
            String noiseStr = cmd.substring(noisePos + 6);
            noise_percentage = noiseStr.toFloat();
            add_noise = (noise_percentage > 0);
            Serial.print("Testing all points with ");
            Serial.print(noise_percentage);
            Serial.println("% noise");
        } else {
            add_noise = false;
            Serial.println("Testing all points without noise");
        }
        
        // Run through all test points
        for (int i = 0; i < NUM_TEST_POINTS; i++) {
            float tagX = test_points[i][0];
            float tagY = test_points[i][1];
            
            Serial.print("\n----- TEST POINT ");
            Serial.print(i + 1);
            Serial.print(" of ");
            Serial.print(NUM_TEST_POINTS);
            Serial.print(": (");
            Serial.print(tagX);
            Serial.print(", ");
            Serial.print(tagY);
            Serial.println(") -----");
            
            // Calculate ideal distances to anchors
            dist_to_a0 = calculateDistance(tagX, tagY, anchor_x[0], anchor_y[0]);
            dist_to_a1 = calculateDistance(tagX, tagY, anchor_x[1], anchor_y[1]);
            dist_to_a2 = calculateDistance(tagX, tagY, anchor_x[2], anchor_y[2]);
            dist_to_a3 = calculateDistance(tagX, tagY, anchor_x[3], anchor_y[3]);
            
            // Apply noise if enabled
            if (add_noise) {
                applyNoise();
            }
            
            Serial.print("Distance to A0: ");
            Serial.println(dist_to_a0);
            Serial.print("Distance to A1: ");
            Serial.println(dist_to_a1);
            Serial.print("Distance to A2: ");
            Serial.println(dist_to_a2);
            Serial.print("Distance to A3: ");
            Serial.println(dist_to_a3);
            
            // Generate and parse AT command
            String atResp = generateATResponse(dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3);
            Serial.print("AT Response: ");
            Serial.println(atResp);
            
            // Parse the AT response to test our parser
            parseRangeData(atResp);
            
            // Calculate position error
            float errorX = abs(positionX - tagX);
            float errorY = abs(positionY - tagY);
            float errorTotal = sqrt(errorX*errorX + errorY*errorY);
            
            Serial.print("Position error: X=");
            Serial.print(errorX);
            Serial.print(", Y=");
            Serial.print(errorY);
            Serial.print(", Total=");
            Serial.print(errorTotal);
            Serial.println(" cm");
            
            delay(500);  // Short pause between test points
        }
    }
    else if (cmd.startsWith("position")) {
        // Parse position coordinates
        int commaPos = cmd.indexOf(",", 9);  // Start after "position "
        if (commaPos > 0) {
            float tagX = cmd.substring(9, commaPos).toFloat();
            float tagY = cmd.substring(commaPos + 1).toFloat();
            
            Serial.print("\n----- Testing specific position: (");
            Serial.print(tagX);
            Serial.print(", ");
            Serial.print(tagY);
            Serial.println(") -----");
            
            // Calculate ideal distances to anchors
            dist_to_a0 = calculateDistance(tagX, tagY, anchor_x[0], anchor_y[0]);
            dist_to_a1 = calculateDistance(tagX, tagY, anchor_x[1], anchor_y[1]);
            dist_to_a2 = calculateDistance(tagX, tagY, anchor_x[2], anchor_y[2]);
            dist_to_a3 = calculateDistance(tagX, tagY, anchor_x[3], anchor_y[3]);
            
            // Apply noise if enabled
            if (add_noise) {
                applyNoise();
            }
            
            Serial.print("Distance to A0: ");
            Serial.println(dist_to_a0);
            Serial.print("Distance to A1: ");
            Serial.println(dist_to_a1);
            Serial.print("Distance to A2: ");
            Serial.println(dist_to_a2);
            Serial.print("Distance to A3: ");
            Serial.println(dist_to_a3);
            
            // Generate and parse AT command
            String atResp = generateATResponse(dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3);
            Serial.print("AT Response: ");
            Serial.println(atResp);
            
            // Parse the AT response to test our parser
            parseRangeData(atResp);
            
            // Calculate position error
            float errorX = abs(positionX - tagX);
            float errorY = abs(positionY - tagY);
            float errorTotal = sqrt(errorX*errorX + errorY*errorY);
            
            Serial.print("Position error: X=");
            Serial.print(errorX);
            Serial.print(", Y=");
            Serial.print(errorY);
            Serial.print(", Total=");
            Serial.print(errorTotal);
            Serial.println(" cm");
        } else {
            Serial.println("Invalid position format. Use: position X,Y");
        }
    }
    else if (cmd == "at") {
        // Generate AT command for current distances
        String atResp = generateATResponse(dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3);
        Serial.println(atResp);
    }
    else if (cmd == "") {
        // Ignore empty commands
    }
    else {
        Serial.println("Unknown command. Type 'help' for available commands.");
    }
}

void loop() {
    if (Serial.available() > 0) {
        processSerialCommand();
    }
    
    // Add a small delay to prevent overwhelming the serial monitor
    delay(10);
}
