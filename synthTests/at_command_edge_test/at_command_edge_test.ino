/*
ESP32S3 UWB Tag - AT Command Edge Case Tester
This sketch tests the multilateration algorithm with challenging test cases,
including edge positions, high noise levels, and error conditions.

PURPOSE:
This file focuses on edge cases and challenging scenarios for the multilateration 
algorithm. It helps identify potential weaknesses and failure modes in the position 
calculation code under non-ideal conditions.

FEATURES:
- Tests positions near corners and edges of the room
- Tests inconsistent measurements that violate triangle inequality
- Simulates signal loss from anchors
- Tests positions outside the valid boundary
- Provides error reporting as both absolute distance and percentage of room size
- Offers a suite of predefined edge cases and the ability to run custom tests

TEST CASES INCLUDED:
1. Edge positions (tag near borders or corners)
2. High noise levels (simulating RF interference or obstacles)
3. Missing anchor data (simulating signal loss)
4. Inconsistent distances (violating triangle inequality)
5. Out-of-bounds positions

USAGE:
- Run this sketch on an ESP32 and open serial monitor
- Type 'help' for available commands
- 'run' executes the complete edge case test suite
- 'test X,Y,A,B,C,D' tests position (X,Y) with noise factors A,B,C,D
- 'at' generates an AT command for current distances

All distances are in centimeters.
*/

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

// Distance measurements to anchors
float dist_to_a0 = 0.0;
float dist_to_a1 = 0.0;
float dist_to_a2 = 0.0;
float dist_to_a3 = 0.0;

// Position of the tag
float positionX = 0.0;
float positionY = 0.0;

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
    
    randomSeed(analogRead(0));  // Initialize random seed
    
    Serial.println("\n\n----- UWB POSITION EDGE CASE TESTER -----");
    Serial.println("This sketch tests the multilateration algorithm with challenging scenarios.");
    Serial.println("Type 'run' to start the test suite or 'help' for more commands.");
}

void testEdgeCase(const char* name, float tagX, float tagY, float noiseA0 = 1.0, float noiseA1 = 1.0, 
                  float noiseA2 = 1.0, float noiseA3 = 1.0) {
    Serial.println("\n-----------------------------------------------------");
    Serial.print("TEST CASE: ");
    Serial.println(name);
    Serial.print("Tag position: (");
    Serial.print(tagX);
    Serial.print(", ");
    Serial.print(tagY);
    Serial.println(")");
    
    // Calculate ideal distances
    float ideal_d0 = calculateDistance(tagX, tagY, anchor_x[0], anchor_y[0]);
    float ideal_d1 = calculateDistance(tagX, tagY, anchor_x[1], anchor_y[1]);
    float ideal_d2 = calculateDistance(tagX, tagY, anchor_x[2], anchor_y[2]);
    float ideal_d3 = calculateDistance(tagX, tagY, anchor_x[3], anchor_y[3]);
    
    // Apply specified noise factors
    dist_to_a0 = ideal_d0 * noiseA0;
    dist_to_a1 = ideal_d1 * noiseA1;
    dist_to_a2 = ideal_d2 * noiseA2;
    dist_to_a3 = ideal_d3 * noiseA3;
    
    Serial.print("Distances (with noise factors ");
    Serial.print(noiseA0);
    Serial.print(", ");
    Serial.print(noiseA1);
    Serial.print(", ");
    Serial.print(noiseA2);
    Serial.print(", ");
    Serial.print(noiseA3);
    Serial.println("):");
    
    Serial.print("A0: ");
    Serial.print(dist_to_a0);
    Serial.print(" (ideal: ");
    Serial.print(ideal_d0);
    Serial.println(")");
    
    Serial.print("A1: ");
    Serial.print(dist_to_a1);
    Serial.print(" (ideal: ");
    Serial.print(ideal_d1);
    Serial.println(")");
    
    Serial.print("A2: ");
    Serial.print(dist_to_a2);
    Serial.print(" (ideal: ");
    Serial.print(ideal_d2);
    Serial.println(")");
    
    Serial.print("A3: ");
    Serial.print(dist_to_a3);
    Serial.print(" (ideal: ");
    Serial.print(ideal_d3);
    Serial.println(")");
    
    // Generate AT command
    String atResp = generateATResponse(dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3);
    Serial.print("AT Response: ");
    Serial.println(atResp);
    
    // Parse response and calculate position
    parseRangeData(atResp);
    
    // Calculate error
    if (positionX != 0.0 || positionY != 0.0) {  // Only if we got a valid position
        float errorX = abs(positionX - tagX);
        float errorY = abs(positionY - tagY);
        float errorTotal = sqrt(errorX*errorX + errorY*errorY);
        
        Serial.print("Position error: X=");
        Serial.print(errorX);
        Serial.print(" cm, Y=");
        Serial.print(errorY);
        Serial.print(" cm, Total=");
        Serial.print(errorTotal);
        Serial.println(" cm");
        
        // Calculate error as percentage of room diagonal
        float roomDiagonal = sqrt(380*380 + 600*600);  // ~710 cm
        float errorPercent = (errorTotal / roomDiagonal) * 100;
        Serial.print("Error as % of room size: ");
        Serial.print(errorPercent);
        Serial.println("%");
    }
}

void runEdgeCaseTests() {
    Serial.println("\n***** STARTING EDGE CASE TEST SUITE *****\n");
    
    // Test Case 1: Normal case in center (baseline)
    testEdgeCase("Center (no noise)", 190, 300);
    delay(500);
    
    // Test Case 2: Moderate noise in center
    testEdgeCase("Center (5% noise)", 190, 300, 1.05, 0.95, 1.05, 0.95);
    delay(500);
    
    // Test Case 3: High noise in center
    testEdgeCase("Center (15% noise)", 190, 300, 1.15, 0.85, 1.10, 0.90);
    delay(500);
    
    // Test Case 4: Near corner (Top Left)
    testEdgeCase("Near Top Left Corner", 20, 20);
    delay(500);
    
    // Test Case 5: Near corner with noise
    testEdgeCase("Near Top Left Corner (10% noise)", 20, 20, 1.10, 0.90, 1.05, 0.95);
    delay(500);
    
    // Test Case 6: Directly on edge
    testEdgeCase("On Left Edge", 0, 300);
    delay(500);
    
    // Test Case 7: Inconsistent measurements (triangle inequality violation)
    // This simulates a reflection or obstacle causing one distance to be much longer
    testEdgeCase("Inconsistent Measurement", 190, 300, 1.0, 1.0, 1.0, 1.9);
    delay(500);
    
    // Test Case 8: Missing anchor (simulate signal loss)
    testEdgeCase("Missing Anchor Data", 190, 300, 1.0, 1.0, 1.0, 0.0);
    delay(500);
    
    // Test Case 9: Outside the boundary
    testEdgeCase("Outside Boundary", 400, 300);
    delay(500);
    
    // Test Case 10: Far outside with erratic measurements
    testEdgeCase("Far Outside with Bad Data", 500, 700, 0.8, 1.2, 0.7, 1.3);
    delay(500);
    
    Serial.println("\n***** EDGE CASE TEST SUITE COMPLETED *****");
}

void processSerialCommand() {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "help") {
        Serial.println("\n----- Available Commands -----");
        Serial.println("run               - Run the complete edge case test suite");
        Serial.println("test X,Y,A,B,C,D  - Test position (X,Y) with noise factors A,B,C,D");
        Serial.println("                    Example: test 100,200,1.1,0.9,1.0,1.0");
        Serial.println("at                - Generate AT command for current distances");
        Serial.println("----------------");
    }
    else if (cmd == "run") {
        runEdgeCaseTests();
    }
    else if (cmd.startsWith("test")) {
        // Format: test X,Y[,A,B,C,D]
        // Parse test parameters
        cmd = cmd.substring(5);  // Remove "test "
        
        // Find commas
        int commaPos[5];
        int commaCount = 0;
        int startPos = 0;
        
        // Find up to 5 commas
        for (int i = 0; i < 5 && commaCount < 5; i++) {
            commaPos[commaCount] = cmd.indexOf(',', startPos);
            if (commaPos[commaCount] >= 0) {
                startPos = commaPos[commaCount] + 1;
                commaCount++;
            } else {
                break;
            }
        }
        
        if (commaCount >= 1) {
            float tagX = cmd.substring(0, commaPos[0]).toFloat();
            float tagY = (commaCount >= 2) ? 
                        cmd.substring(commaPos[0] + 1, commaPos[1]).toFloat() :
                        cmd.substring(commaPos[0] + 1).toFloat();
            
            // Default noise factors
            float noiseA0 = 1.0, noiseA1 = 1.0, noiseA2 = 1.0, noiseA3 = 1.0;
            
            // If we have noise factors specified
            if (commaCount >= 5) {
                noiseA0 = cmd.substring(commaPos[1] + 1, commaPos[2]).toFloat();
                noiseA1 = cmd.substring(commaPos[2] + 1, commaPos[3]).toFloat();
                noiseA2 = cmd.substring(commaPos[3] + 1, commaPos[4]).toFloat();
                noiseA3 = cmd.substring(commaPos[4] + 1).toFloat();
            }
            
            testEdgeCase("Custom Test", tagX, tagY, noiseA0, noiseA1, noiseA2, noiseA3);
        } else {
            Serial.println("Invalid test format. Use: test X,Y[,A,B,C,D]");
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
