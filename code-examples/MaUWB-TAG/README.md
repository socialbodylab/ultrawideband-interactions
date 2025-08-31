# MaUWB-TAG - Object-Oriented UWB Tag Implementation

A modular, class-based Arduino implementation for UWB (Ultra-Wideband) tag functionality that provides distance measurement and position calculation using advanced multilateration algorithms.

## Features

- **Object-oriented design** for easy integration and reuse
- **Configurable parameters** (tag index, refresh rate, max tags)
- **Expandable anchor system** with dynamic positioning (up to 10 anchors)
- **Real-time distance measurement** from UWB anchors
- **Advanced multilateration** for accurate 2D position calculation
- **Position filtering and smoothing** with configurable history length
- **OLED display integration** with customizable refresh rates
- **Event callbacks** for position and distance updates
- **Hardware abstraction** for easy porting to different platforms

## Hardware Requirements

- ESP32S3 development board
- UWB module with AT command interface
- SSD1306 OLED display (128x64)
- I2C connections for display
- UART connections for UWB module

### Pin Configuration (ESP32S3)

```cpp
#define RESET 16      // UWB module reset pin
#define IO_RXD2 18    // UWB module RX
#define IO_TXD2 17    // UWB module TX
#define I2C_SDA 39    // Display SDA
#define I2C_SCL 38    // Display SCL
```

## Library Dependencies

- Wire (2.0.0)
- Adafruit_GFX_Library (1.11.7)
- Adafruit_BusIO (1.14.4)
- SPI (2.0.0)
- Adafruit_SSD1306 (2.5.7)

## Quick Start

### Basic Usage

```cpp
#include "MaUWB_TAG.h"

// Create tag instance
MaUWB_TAG uwbTag(7, 50);  // Tag index 7, 50ms refresh rate

void setup() {
    uwbTag.begin();
}

void loop() {
    uwbTag.update();
    
    // Access position data
    if (uwbTag.hasValidPosition()) {
        float x = uwbTag.getPositionX();
        float y = uwbTag.getPositionY();
        // Use position data...
    }
}
```

### Advanced Configuration

```cpp
void setup() {
    // Configure before calling begin()
    uwbTag.setDisplayRefreshRate(100);     // 100ms display updates
    uwbTag.setMaxTags(15);                 // Support 15 tags
    uwbTag.setPositionHistoryLength(5);    // Average 5 positions
    
    // Set anchor positions using convenience methods
    uwbTag.anchor0(0, 0);        // Top-left
    uwbTag.anchor1(0, 600);      // Bottom-left
    uwbTag.anchor2(380, 600);    // Bottom-right
    uwbTag.anchor3(380, 0);      // Top-right
    
    // Or use the generic method
    // uwbTag.setAnchorPosition(0, 0, 0);
    
    uwbTag.begin();
}
```

## Class Methods

### Constructor
```cpp
MaUWB_TAG(uint8_t tagIndex, unsigned long refreshRate = 50)
```
- `tagIndex`: Unique identifier for this tag (0-255)
- `refreshRate`: Rate to request new distance data (milliseconds)

### Initialization
```cpp
bool begin()
```
Initializes hardware and configures the UWB module. Returns `true` on success.

### Main Update
```cpp
void update()
```
Main update function - call this in your `loop()`. Handles serial communication, position calculation, and display updates.

### Configuration Methods
```cpp
void setDisplayRefreshRate(unsigned long intervalMs)
void setMaxTags(uint8_t maxTags)
void setPositionHistoryLength(uint8_t length)

// Debug control
void enableDebug(bool enable = true)
void disableDebug()
bool isDebugEnabled() const
```

### Anchor Management
```cpp
void setAnchorCount(uint8_t count)
void setAnchorPosition(uint8_t anchorIndex, float x, float y)
void setDefaultAnchors()  // Sets standard 4-anchor rectangular setup

// Convenience methods for individual anchors (0-9)
void anchor0(float x, float y)
void anchor1(float x, float y)
void anchor2(float x, float y)
void anchor3(float x, float y)
// ... up to anchor9(float x, float y)
```

### Data Access
```cpp
float getPositionX() const
float getPositionY() const
float getDistance(uint8_t anchorIndex) const
bool hasValidPosition() const
```

### Event Callbacks
Override these methods in a derived class for custom behavior:

```cpp
virtual void onPositionUpdate(float x, float y)
virtual void onDistanceUpdate(uint8_t anchorIndex, float distance)
```

## Examples

### 1. Basic Tag (`MaUWB-TAG.ino`)
Simple implementation with default 4-anchor setup.

### 2. Advanced Usage (`example_advanced.ino`)
Demonstrates:
- Custom anchor configurations
- Event handling for zones and proximity
- Extended functionality with derived class

## Default Anchor Configuration

The class includes a default 4-anchor rectangular setup:

```
A3 (380,0) -------- A0 (0,0)
    |                   |
    |                   |
    |                   |
A2 (380,600) ------ A1 (0,600)
```

## Position Calculation

The implementation uses advanced multilateration techniques:

1. **Multiple triplet calculations** - Uses all possible combinations of 3+ anchors
2. **Solution averaging** - Averages valid solutions for improved accuracy
3. **Boundary validation** - Filters out impossible positions
4. **Position smoothing** - Optional rolling average for stable positioning

## Debugging

### Debug Control

Debug output is **disabled by default** for optimal performance. You can enable it in several ways:

#### Compile-time Debug
Uncomment the `#define DEBUG_MODE` line at the top of the file to enable debug by default.

#### Runtime Debug Control
```cpp
// Enable debug output
uwbTag.enableDebug();

// Disable debug output  
uwbTag.disableDebug();

// Check debug status
if (uwbTag.isDebugEnabled()) {
    // Debug is currently enabled
}
```

#### Interactive Debug Control
The main example includes serial commands to toggle debug:
- Send `'d'` or `'D'` to enable debug
- Send `'q'` or `'Q'` to disable debug

### Debug Output Includes
- Range requests and responses
- Distance measurements from anchors
- Position calculations
- UWB module configuration commands
- Serial communication details


