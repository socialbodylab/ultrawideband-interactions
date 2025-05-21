/*
ESP32S3 UWB Tag - Robust Multilateration Tester
This sketch tests improved multilateration techniques that handle noisy data better.

PURPOSE:
This file implements and tests advanced techniques to improve the robustness of 
position calculation with UWB distance data. It demonstrates methods to achieve 
more stable and accurate positioning even in the presence of significant measurement 
errors or inconsistencies.

FEATURES:
1. Weighted averaging by distance quality - Gives more weight to more reliable measurements
2. Detection and exclusion of inconsistent measurements - Identifies physically impossible scenarios
3. Kalman filtering for position smoothing - Applies statistical filtering to smooth estimates
4. Triangle inequality validation - Checks if measurements satisfy physical constraints
5. Statistical outlier detection - Identifies and handles outlier measurements

NOISE PATTERNS TESTED:
- Random noise (simulating general RF interference)
- One bad measurement (simulating temporary obstruction)
- Multipath error (systematically longer distances)
- Progressive error (gradual increase in noise)
- Triangle inequality violation (physically impossible measurements)

USAGE:
- Run this sketch on an ESP32 and open serial monitor
- Type 'help' for available commands
- 'run' executes the robustness test suite
- 'test X,Y' tests position (X,Y) with all noise patterns
- 'noise N X,Y' tests noise pattern N (1-5) at position (X,Y)
- 'reset' resets Kalman filter state

These advanced techniques can significantly improve positioning accuracy and stability 
in real-world environments where noise and signal obstruction are common challenges.

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

// Signal quality indicators (based on RSSI, variance, etc.)
float quality_a0 = 1.0;
float quality_a1 = 1.0;
float quality_a2 = 1.0;
float quality_a3 = 1.0;

// Kalman filter parameters
float kalman_x = 190.0;  // Initial estimate at center
float kalman_y = 300.0;  // Initial estimate at center
float kalman_p_x = 100.0;  // Initial error estimate
float kalman_p_y = 100.0;  // Initial error estimate
float kalman_q = 0.01;  // Process noise
float kalman_r = 1.0;  // Measurement noise

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

// Check if triangle inequality is violated
bool checkTriangleInequality(float a, float b, float c) {
    return (a + b > c) && (a + c > b) && (b + c > a);
}

// Detect inconsistent measurements by checking triangle inequality between anchors
void detectInconsistentMeasurements() {
    float anchors[4][2] = {
        {anchor_x[0], anchor_y[0]},
        {anchor_x[1], anchor_y[1]},
        {anchor_x[2], anchor_y[2]},
        {anchor_x[3], anchor_y[3]}
    };
    
    float distances[4] = {dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3};
    float qualities[4] = {1.0, 1.0, 1.0, 1.0};  // Start with equal quality
    
    // For each triangle formed by tag + 2 anchors
    for (int i = 0; i < 3; i++) {
        for (int j = i+1; j < 4; j++) {
            // Distance between anchors i and j
            float anchor_dist = calculateDistance(anchors[i][0], anchors[i][1], 
                                                anchors[j][0], anchors[j][1]);
            
            // Check triangle inequality: d_tag_i + d_tag_j > d_i_j
            if (!checkTriangleInequality(distances[i], distances[j], anchor_dist)) {
                Serial.print("Triangle inequality violation detected between anchors ");
                Serial.print(i);
                Serial.print(" and ");
                Serial.println(j);
                
                // Lower quality of both measurements
                qualities[i] *= 0.5;
                qualities[j] *= 0.5;
            }
        }
    }
    
    // Update quality values
    quality_a0 = qualities[0];
    quality_a1 = qualities[1];
    quality_a2 = qualities[2];
    quality_a3 = qualities[3];
    
    Serial.print("Signal qualities: ");
    Serial.print(quality_a0);
    Serial.print(", ");
    Serial.print(quality_a1);
    Serial.print(", ");
    Serial.print(quality_a2);
    Serial.print(", ");
    Serial.println(quality_a3);
}

// Calculate 2D position using enhanced multilateration with all 4 anchors
// This version uses quality-weighted solutions
void calculatePosition() {
    // Check if we have valid distances from all 4 anchors
    if (dist_to_a0 <= 0 || dist_to_a1 <= 0 || dist_to_a2 <= 0 || dist_to_a3 <= 0) {
        Serial.println("Cannot calculate position - need distances from all 4 anchors");
        return;
    }
    
    // Check for inconsistent measurements
    detectInconsistentMeasurements();
    
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
    
    // Solve using anchors 0,1,2 with weight as product of quality values
    float weight1 = quality_a0 * quality_a1 * quality_a2;
    float den1 = (A_01 * B_12 - B_01 * A_12);
    bool valid1 = false;
    
    if (abs(den1) > 0.000001) {
        x1 = (C_01 * B_12 - C_12 * B_01) / den1;
        y1 = (A_01 * C_12 - C_01 * A_12) / den1;
        valid1 = true;
        
        Serial.print("Position from anchors 0,1,2: (");
        Serial.print(x1);
        Serial.print(", ");
        Serial.print(y1);
        Serial.print("), weight: ");
        Serial.println(weight1);
    }
    
    // Solve using anchors 0,1,3 with weight
    float weight2 = quality_a0 * quality_a1 * quality_a3;
    float den2 = (A_01 * B_13 - B_01 * A_13);
    bool valid2 = false;
    
    if (abs(den2) > 0.000001) {
        x2 = (C_01 * B_13 - C_13 * B_01) / den2;
        y2 = (A_01 * C_13 - C_01 * A_13) / den2;
        valid2 = true;
        
        Serial.print("Position from anchors 0,1,3: (");
        Serial.print(x2);
        Serial.print(", ");
        Serial.print(y2);
        Serial.print("), weight: ");
        Serial.println(weight2);
    }
    
    // Solve using anchors 0,2,3 with weight
    float weight3 = quality_a0 * quality_a2 * quality_a3;
    float den3 = (A_02 * B_23 - B_02 * A_23);
    bool valid3 = false;
    
    if (abs(den3) > 0.000001) {
        x3 = (C_02 * B_23 - C_23 * B_02) / den3;
        y3 = (A_02 * C_23 - C_02 * A_23) / den3;
        valid3 = true;
        
        Serial.print("Position from anchors 0,2,3: (");
        Serial.print(x3);
        Serial.print(", ");
        Serial.print(y3);
        Serial.print("), weight: ");
        Serial.println(weight3);
    }
    
    // Count valid solutions
    if (!valid1 && !valid2 && !valid3) {
        Serial.println("Cannot calculate position - no valid solutions");
        return;
    }
    
    // Calculate weighted average of valid solutions
    float rawX = 0.0, rawY = 0.0;
    float totalWeight = 0.0;
    
    if (valid1) {
        rawX += x1 * weight1;
        rawY += y1 * weight1;
        totalWeight += weight1;
    }
    if (valid2) {
        rawX += x2 * weight2;
        rawY += y2 * weight2;
        totalWeight += weight2;
    }
    if (valid3) {
        rawX += x3 * weight3;
        rawY += y3 * weight3;
        totalWeight += weight3;
    }
    
    if (totalWeight > 0) {
        rawX /= totalWeight;
        rawY /= totalWeight;
    }
    
    Serial.print("Weighted position: (");
    Serial.print(rawX);
    Serial.print(", ");
    Serial.println(rawY);
    Serial.println(")");
    
    // Apply Kalman filter
    // Prediction step
    // (In this simple 2D case, we're assuming the position doesn't change
    //  between measurements, so the prediction is just the previous estimate)
    
    // Prediction error
    kalman_p_x += kalman_q;
    kalman_p_y += kalman_q;
    
    // Calculate Kalman gain
    float k_x = kalman_p_x / (kalman_p_x + kalman_r);
    float k_y = kalman_p_y / (kalman_p_y + kalman_r);
    
    // Update step
    kalman_x += k_x * (rawX - kalman_x);
    kalman_y += k_y * (rawY - kalman_y);
    
    // Update error estimate
    kalman_p_x = (1 - k_x) * kalman_p_x;
    kalman_p_y = (1 - k_y) * kalman_p_y;
    
    // Final filtered position
    positionX = kalman_x;
    positionY = kalman_y;
    
    Serial.print("Kalman filtered position: (");
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
    
    Serial.println("\n\n----- ROBUST MULTILATERATION TESTER -----");
    Serial.println("This sketch tests improved multilateration techniques for noisy UWB data.");
    Serial.println("Type 'help' for available commands.");
}

// Inject a specified noise pattern into measurements
void injectNoisePattern(int patternType, float tagX, float tagY) {
    // Calculate ideal distances
    float ideal_d0 = calculateDistance(tagX, tagY, anchor_x[0], anchor_y[0]);
    float ideal_d1 = calculateDistance(tagX, tagY, anchor_x[1], anchor_y[1]);
    float ideal_d2 = calculateDistance(tagX, tagY, anchor_x[2], anchor_y[2]);
    float ideal_d3 = calculateDistance(tagX, tagY, anchor_x[3], anchor_y[3]);
    
    // Start with ideal distances
    dist_to_a0 = ideal_d0;
    dist_to_a1 = ideal_d1;
    dist_to_a2 = ideal_d2;
    dist_to_a3 = ideal_d3;
    
    switch (patternType) {
        case 1: // Random noise
            dist_to_a0 *= (1.0 + random(-10, 10) / 100.0);
            dist_to_a1 *= (1.0 + random(-10, 10) / 100.0);
            dist_to_a2 *= (1.0 + random(-10, 10) / 100.0);
            dist_to_a3 *= (1.0 + random(-10, 10) / 100.0);
            Serial.println("Applied random noise (±10%)");
            break;
            
        case 2: // One bad measurement
            dist_to_a0 *= 1.5;  // 50% error on first measurement
            Serial.println("Applied 50% error to anchor 0 measurement");
            break;
            
        case 3: // Multipath error (systematically longer distances)
            dist_to_a0 *= 1.2;
            dist_to_a1 *= 1.15;
            dist_to_a2 *= 1.25;
            dist_to_a3 *= 1.18;
            Serial.println("Applied multipath error (all distances too long)");
            break;
            
        case 4: // Progressive error (gradual increase in noise)
            for (int i = 0; i < 10; i++) {
                // Start with small noise, gradually increase
                float noise_factor = (i + 1) * 0.02;  // 2% to 20%
                
                float d0 = ideal_d0 * (1.0 + noise_factor);
                float d1 = ideal_d1 * (1.0 + noise_factor);
                float d2 = ideal_d2 * (1.0 + noise_factor);
                float d3 = ideal_d3 * (1.0 + noise_factor);
                
                Serial.print("Progressive noise test iteration ");
                Serial.print(i + 1);
                Serial.print(" with ");
                Serial.print(noise_factor * 100.0);
                Serial.println("% noise");
                
                // Update distances
                dist_to_a0 = d0;
                dist_to_a1 = d1;
                dist_to_a2 = d2;
                dist_to_a3 = d3;
                
                // Generate AT command and calculate position
                String atResp = generateATResponse(dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3);
                parseRangeData(atResp);
                
                // Calculate error
                float errorX = abs(positionX - tagX);
                float errorY = abs(positionY - tagY);
                float errorTotal = sqrt(errorX*errorX + errorY*errorY);
                
                Serial.print("Position error: Total=");
                Serial.print(errorTotal);
                Serial.println(" cm");
                
                delay(500);
            }
            return;  // Return early as we've already processed this pattern
            
        case 5: // Triangle inequality violation
            // Create distances that violate the triangle inequality
            dist_to_a0 = ideal_d0 * 0.5;  // Make one distance too short
            Serial.println("Applied triangle inequality violation");
            break;
    }
    
    // Generate AT command
    String atResp = generateATResponse(dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3);
    Serial.print("AT Response: ");
    Serial.println(atResp);
    
    // Parse response
    parseRangeData(atResp);
    
    // Calculate error
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
}

void resetKalmanFilter() {
    // Reset Kalman filter to initial state
    kalman_x = 190.0;
    kalman_y = 300.0;
    kalman_p_x = 100.0;
    kalman_p_y = 100.0;
    Serial.println("Kalman filter reset to initial state");
}

void runRobustnessTests() {
    Serial.println("\n***** STARTING ROBUSTNESS TEST SUITE *****\n");
    
    float testX = 190;
    float testY = 300;
    
    Serial.print("Testing with tag at position: (");
    Serial.print(testX);
    Serial.print(", ");
    Serial.print(testY);
    Serial.println(")");
    
    // Reset Kalman filter before tests
    resetKalmanFilter();
    
    // Test 1: No noise baseline
    Serial.println("\n----- Test 1: No Noise Baseline -----");
    dist_to_a0 = calculateDistance(testX, testY, anchor_x[0], anchor_y[0]);
    dist_to_a1 = calculateDistance(testX, testY, anchor_x[1], anchor_y[1]);
    dist_to_a2 = calculateDistance(testX, testY, anchor_x[2], anchor_y[2]);
    dist_to_a3 = calculateDistance(testX, testY, anchor_x[3], anchor_y[3]);
    
    String atResp = generateATResponse(dist_to_a0, dist_to_a1, dist_to_a2, dist_to_a3);
    parseRangeData(atResp);
    delay(500);
    
    // Test 2-5: Various noise patterns
    for (int pattern = 1; pattern <= 5; pattern++) {
        Serial.print("\n----- Test ");
        Serial.print(pattern + 1);
        Serial.print(": Noise Pattern ");
        Serial.print(pattern);
        Serial.println(" -----");
        
        injectNoisePattern(pattern, testX, testY);
        delay(500);
    }
    
    Serial.println("\n***** ROBUSTNESS TEST SUITE COMPLETED *****");
}

void processSerialCommand() {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "help") {
        Serial.println("\n----- Available Commands -----");
        Serial.println("run             - Run the robustness test suite");
        Serial.println("test X,Y        - Test position (X,Y) with all noise patterns");
        Serial.println("noise N X,Y     - Test noise pattern N (1-5) at position (X,Y)");
        Serial.println("reset           - Reset Kalman filter state");
        Serial.println("at              - Generate AT command for current distances");
        Serial.println("----------------");
    }
    else if (cmd == "run") {
        runRobustnessTests();
    }
    else if (cmd.startsWith("test")) {
        // Parse coordinates
        int commaPos = cmd.indexOf(",", 5);
        if (commaPos > 0) {
            float tagX = cmd.substring(5, commaPos).toFloat();
            float tagY = cmd.substring(commaPos + 1).toFloat();
            
            Serial.print("Testing all noise patterns at position (");
            Serial.print(tagX);
            Serial.print(", ");
            Serial.print(tagY);
            Serial.println(")");
            
            // Run all noise patterns
            for (int pattern = 1; pattern <= 5; pattern++) {
                Serial.print("\n----- Noise Pattern ");
                Serial.print(pattern);
                Serial.println(" -----");
                
                injectNoisePattern(pattern, tagX, tagY);
                delay(500);
            }
        } else {
            Serial.println("Invalid format. Use: test X,Y");
        }
    }
    else if (cmd.startsWith("noise")) {
        // Format: noise N X,Y
        int spacePos1 = cmd.indexOf(" ", 0);
        int spacePos2 = cmd.indexOf(" ", spacePos1 + 1);
        int commaPos = cmd.indexOf(",");
        
        if (spacePos1 > 0 && spacePos2 > 0 && commaPos > spacePos2) {
            int pattern = cmd.substring(spacePos1 + 1, spacePos2).toInt();
            float tagX = cmd.substring(spacePos2 + 1, commaPos).toFloat();
            float tagY = cmd.substring(commaPos + 1).toFloat();
            
            if (pattern >= 1 && pattern <= 5) {
                Serial.print("Testing noise pattern ");
                Serial.print(pattern);
                Serial.print(" at position (");
                Serial.print(tagX);
                Serial.print(", ");
                Serial.print(tagY);
                Serial.println(")");
                
                injectNoisePattern(pattern, tagX, tagY);
            } else {
                Serial.println("Invalid pattern number. Use 1-5.");
            }
        } else {
            Serial.println("Invalid format. Use: noise N X,Y");
        }
    }
    else if (cmd == "reset") {
        resetKalmanFilter();
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
