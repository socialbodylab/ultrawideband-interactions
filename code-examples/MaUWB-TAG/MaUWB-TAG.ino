#include "MaUWB_TAG.h"


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
