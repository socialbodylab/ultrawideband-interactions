/*
UWB Position Calculation Test
This sketch tests the multilateration algorithm with synthetic distance data.

PURPOSE:
This is a standalone test program that validates the position calculation algorithm
without requiring actual UWB hardware. It creates simulated distances for a known
position and verifies that the algorithm correctly calculates that position.

FEATURES:
- Tests the basic multilateration algorithm with perfect measurements
- Tests position calculation with simulated noise to evaluate error tolerance
- Calculates and reports position errors compared to the known position
- Uses all 4 anchors for more robust position calculation

SCENARIOS TESTED:
1. Perfect measurements with synthetic distances at position (190, 300)
2. 5% noise simulation to test algorithm robustness

This test serves as the baseline validation of the multilateration approach
before implementing it in the main UWB tag code.
*/

#include <Arduino.h>

// Anchor positions in cm
// A0 is top left (0,0)
// A1 is left bottom (0,600)
// A2 is right bottom (380,600)
// A3 is top right (380,0)
const float anchor_x[4] = {0, 0, 380, 380};
const float anchor_y[4] = {0, 600, 600, 0};

// Distance measurements to anchors (using anchors 0-3)
float dist_to_a0 = 0.0;
float dist_to_a1 = 0.0;
float dist_to_a2 = 0.0;
float dist_to_a3 = 0.0;

// Position of the tag
float positionX = 0.0;
float positionY = 0.0;

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
    
    // Solve using anchors 0,1,2
    float A1 = 2 * (anchor_x[1] - anchor_x[0]);
    float B1 = 2 * (anchor_y[1] - anchor_y[0]);
    float C1 = dist_to_a0 * dist_to_a0 - dist_to_a1 * dist_to_a1 - 
              anchor_x[0] * anchor_x[0] + anchor_x[1] * anchor_x[1] - 
              anchor_y[0] * anchor_y[0] + anchor_y[1] * anchor_y[1];
              
    float D1 = 2 * (anchor_x[2] - anchor_x[1]);
    float E1 = 2 * (anchor_y[2] - anchor_y[1]);
    float F1 = dist_to_a1 * dist_to_a1 - dist_to_a2 * dist_to_a2 -
              anchor_x[1] * anchor_x[1] + anchor_x[2] * anchor_x[2] - 
              anchor_y[1] * anchor_y[1] + anchor_y[2] * anchor_y[2];
    
    float den1 = (A1 * E1 - B1 * D1);
    if (abs(den1) > 0.000001) {
        x1 = (C1 * E1 - F1 * B1) / den1;
        y1 = (A1 * F1 - C1 * D1) / den1;
        Serial.print("Position from anchors 0,1,2: (");
        Serial.print(x1);
        Serial.print(", ");
        Serial.print(y1);
        Serial.println(")");
    }
    
    // Solve using anchors 0,1,3
    float A2 = 2 * (anchor_x[1] - anchor_x[0]);
    float B2 = 2 * (anchor_y[1] - anchor_y[0]);
    float C2 = dist_to_a0 * dist_to_a0 - dist_to_a1 * dist_to_a1 - 
              anchor_x[0] * anchor_x[0] + anchor_x[1] * anchor_x[1] - 
              anchor_y[0] * anchor_y[0] + anchor_y[1] * anchor_y[1];
              
    float D2 = 2 * (anchor_x[3] - anchor_x[1]);
    float E2 = 2 * (anchor_y[3] - anchor_y[1]);
    float F2 = dist_to_a1 * dist_to_a1 - dist_to_a3 * dist_to_a3 -
              anchor_x[1] * anchor_x[1] + anchor_x[3] * anchor_x[3] - 
              anchor_y[1] * anchor_y[1] + anchor_y[3] * anchor_y[3];
    
    float den2 = (A2 * E2 - B2 * D2);
    if (abs(den2) > 0.000001) {
        x2 = (C2 * E2 - F2 * B2) / den2;
        y2 = (A2 * F2 - C2 * D2) / den2;
        Serial.print("Position from anchors 0,1,3: (");
        Serial.print(x2);
        Serial.print(", ");
        Serial.print(y2);
        Serial.println(")");
    }
    
    // Solve using anchors 0,2,3
    float A3 = 2 * (anchor_x[2] - anchor_x[0]);
    float B3 = 2 * (anchor_y[2] - anchor_y[0]);
    float C3 = dist_to_a0 * dist_to_a0 - dist_to_a2 * dist_to_a2 - 
              anchor_x[0] * anchor_x[0] + anchor_x[2] * anchor_x[2] - 
              anchor_y[0] * anchor_y[0] + anchor_y[2] * anchor_y[2];
              
    float D3 = 2 * (anchor_x[3] - anchor_x[2]);
    float E3 = 2 * (anchor_y[3] - anchor_y[2]);
    float F3 = dist_to_a2 * dist_to_a2 - dist_to_a3 * dist_to_a3 -
              anchor_x[2] * anchor_x[2] + anchor_x[3] * anchor_x[3] - 
              anchor_y[2] * anchor_y[2] + anchor_y[3] * anchor_y[3];
    
    float den3 = (A3 * E3 - B3 * D3);
    if (abs(den3) > 0.000001) {
        x3 = (C3 * E3 - F3 * B3) / den3;
        y3 = (A3 * F3 - C3 * D3) / den3;
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
    
    // Store the raw calculated position
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

// Calculate distance from (x1,y1) to (x2,y2)
float calculateDistance(float x1, float y1, float x2, float y2) {
    return sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n----- POSITION CALCULATION TEST -----");
    
    // Test case: Tag is in center of the rectangle (190, 300)
    float tagX = 190;
    float tagY = 300;
    
    Serial.print("Testing with tag at position: (");
    Serial.print(tagX);
    Serial.print(", ");
    Serial.print(tagY);
    Serial.println(")");
    
    // Calculate synthetic distances (perfect measurements)
    dist_to_a0 = calculateDistance(tagX, tagY, anchor_x[0], anchor_y[0]);
    dist_to_a1 = calculateDistance(tagX, tagY, anchor_x[1], anchor_y[1]);
    dist_to_a2 = calculateDistance(tagX, tagY, anchor_x[2], anchor_y[2]);
    dist_to_a3 = calculateDistance(tagX, tagY, anchor_x[3], anchor_y[3]);
    
    Serial.print("Distance to A0: ");
    Serial.println(dist_to_a0);
    Serial.print("Distance to A1: ");
    Serial.println(dist_to_a1);
    Serial.print("Distance to A2: ");
    Serial.println(dist_to_a2);
    Serial.print("Distance to A3: ");
    Serial.println(dist_to_a3);
    
    // Calculate position using our multilateration method
    calculatePosition();
    
    // Check accuracy
    float errorX = abs(positionX - tagX);
    float errorY = abs(positionY - tagY);
    float errorTotal = sqrt(errorX*errorX + errorY*errorY);
    
    Serial.print("Position error: X=");
    Serial.print(errorX);
    Serial.print(", Y=");
    Serial.print(errorY);
    Serial.print(", Total=");
    Serial.println(errorTotal);
    
    // Test with noise
    Serial.println("\n----- TESTING WITH 5% NOISE -----");
    
    // Add 5% noise
    dist_to_a0 *= 1.05;
    dist_to_a1 *= 0.95;
    dist_to_a2 *= 1.03;
    dist_to_a3 *= 0.97;
    
    Serial.print("Distance to A0 (with noise): ");
    Serial.println(dist_to_a0);
    Serial.print("Distance to A1 (with noise): ");
    Serial.println(dist_to_a1);
    Serial.print("Distance to A2 (with noise): ");
    Serial.println(dist_to_a2);
    Serial.print("Distance to A3 (with noise): ");
    Serial.println(dist_to_a3);
    
    // Calculate position with noisy data
    calculatePosition();
    
    // Check accuracy with noise
    errorX = abs(positionX - tagX);
    errorY = abs(positionY - tagY);
    errorTotal = sqrt(errorX*errorX + errorY*errorY);
    
    Serial.print("Position error with noise: X=");
    Serial.print(errorX);
    Serial.print(", Y=");
    Serial.print(errorY);
    Serial.print(", Total=");
    Serial.println(errorTotal);
}

void loop() {
    // Nothing to do in loop
    delay(1000);
}
