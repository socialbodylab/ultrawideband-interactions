# P5js Calibration Script

P5js script to calibrate antenna delay of UWB modules via web serial. This sketch connects to the UWB module and
uses the `JSON` data sent from the module to calculate the error compared with the configured distance. The script can update the
antenna delay for the connected _ANCHOR_ on the fly. After finding the appropriate antenna delay value, update the
_ANCHOR_ arduino sketch with the corresponding value and reflash the device(_This only applies to ANCHOR 1-3_).
For _A0_ the module will store the antenna delay, so there is no need to reflash.

> Keep a single _TAG_ running for calibration.

## Using the script

1. Open this html file in Chrome or any browser that supports Web Serial API.
2. Connect the UWB module to the computes using a USB cable.
3. Click the `Connect Anchor` button, you will be prompted to select the serial port.
4. Set the distance between the anchor and tag in centimeters.
5. Select the corresponding _ANCHOR_ index (0-3).
6. Monitor the reading and adjust the Antenna delay accordingly using the `set antenna delay`, `save config` and `restart module` buttons.
7. The UI will notify you when the average error is within 5% the real world distance. (Wait for a moment for the values to stabilize)
