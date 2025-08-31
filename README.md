# ultrawideband-interactions

## Links
- [Makerfabs ESP32S3 UWB Module Wiki](https://wiki.makerfabs.com/MaUWB_ESP32S3%20UWB%20module.html)
- [Makerfabs ESP32S3 UWB GitHub Repository](https://github.com/Makerfabs/MaUWB_ESP32S3-with-STM32-AT-Command)



# Basic Setup for ANCHORs and TAGs

## 1. In Arduino IDE
- Open Board Manager and install **esp32 by Expressif**
- Open Library Manager and Install **Adafruit Screen Library - SSD1306** (with dependencies)

## 2. From the SBL UWB GitHub Repository
- Download the code example files along with their associated folders for:
  - ANCHOR A0 ([ANCHOR_A0](ANCHOR_A0))
  - ANCHORs A1, A2 and A3 ([ANCHOR_default](ANCHOR_default))
  - TAGs ([TAG_xyPosition](TAG_xyPosition))

## 3. Label Your Boards
Use a label maker to name and mark your board (A0, A1, A2, T0, T1, T2, and so on...)

## 4. Configure the UWB Index
In Arduino IDE, under:
```cpp
// User config
```
make sure it says:
```cpp
#define UWB_INDEX 1
```
(or the appropriate integer for the ANCHOR or TAG you need as labelled in the previous step above)

## 5. Flash Firmware
To flash firmware, plug the cable into the USB C port marked **USB NATIVE** (the one next to the two tiny black buttons, NOT the one close to the white JST battery port)

![USB Native Port Connection](/images/setupAndCalibration__Page_1_Image_0001.jpg)

## 6. MacOS Connection
On MacOS, when you connect the board let it connect to the accessory in the pop-up by choosing "Allow"

## 7. Choosing the Board in Arduino IDE
- Under the board drop-down menu, choose **ESP32 Family Device**
- THEN, do the thing below (Tools > Board: "ESP32 Family Device" > esp32 > ESP32S3 Dev Module):

![Arduino IDE Board Selection](/images/setupAndCalibration__Page_1_Image_0002.jpg)

- Make sure your **USB CDC on Boot is Enabled**:

![USB CDC on Boot Setting](/images/setupAndCalibration__Page_2_Image_0001.jpg)

## 8. Verify and Upload
Then, verify and upload the code to the device

## 9. Successful Upload Indicators
When uploaded correctly, the screen on your board will say A3 (or the appropriate ID number) and you will get police lights at the back (1 flashing blue and 2 orange lights) like below:

![Board with Display and LED Indicators](/images/setupAndCalibration__Page_2_Image_0002.jpg)
![Board LED Lights Pattern](/images/setupAndCalibration__Page_2_Image_0004.jpg)

## 10. Mount the ANCHOR Boards
Mount the ANCHOR boards in four corners of the space, making sure to keep them in the following orientation:
- Board **VERTICAL**
- Screen **OUT** (AWAY from the other boards)
- USB ports pointed **UP**
- Boards at the **SAME relative HEIGHT**

## 11. Connect ANCHOR A0
Connect the ANCHOR A0 using the USB port to your computer and open the ANCHOR_A0 file in Arduino IDE to read measurement values.

---

# How to Calibrate the ANCHORs

Calibration should ideally be done each time the ANCHOR-TAG arrangement is set up in a new space to achieve best results.

The idea is to calibrate all ANCHORs with one TAG so that the real-world distance between the two device types matches the reading in the code.

## Calibration Steps

### 1. Flash the Home ANCHOR
Flash the first ANCHOR (A0) with the [ANCHOR A0 code](ANCHOR_A0_code). This is the home ANCHOR and will be used to read values off of when connected to a computer.

### 2. Flash Other ANCHORs
Flash the other ANCHORs (A1, A2, and A3) with the [ANCHOR_default code](ANCHOR_default_code).

### 3. Flash TAGs
Flash all the TAGs with the [TAG code](TAG_code), ensuring to modify the code with the appropriate TAG number (T0, T1, T2 and so on...) at the top of the code (see step 5 in Basic Setup for ANCHORs and TAGs).

### 4. Position TAG for Calibration
Place one TAG a fixed distance away from the ANCHOR making sure that the two are at the same height and completely stationary to ensure accurate readings. A distance of 700-800 cm away is good to shoot for according to [jremington's Calibration Guide](jremington's_Calibration_Guide).

![Calibration Setup Diagram](/images/setupAndCalibration__Page_3_Image_0001.jpg)
![Calibration Setup Photo](/images/setupAndCalibration__Page_3_Image_0002.jpg)

### 5. Connect to Computer
Connect the ANCHOR to your computer using a USB cable.

### 6. Using the p5.js Calibration Tool
Using the [p5.js Calibration sketch](p5.js_Calibration_sketch) shown below:

![UWB Antenna Delay Calibration Tool Interface](/images/setupAndCalibration__Page_4_Image_0001.jpg)

Update the antenna delay. Increasing the antenna delay value will increase the measured distance and decreasing it will decrease the measured distance.

### 7. Adjust Antenna Delay
Adjust this value in steps of ±10 until it shows a reading close to the actual distance of the TAG from the ANCHOR. You can fine tune the value when you get closer to the real-world value. An error of < 5% is acceptable.

### 8. Download Calibration Tool
Download the entire [p5_calibration folder](p5_calibration_folder) and open the HTML file inside in Chrome or any other browser that supports the Web Serial API.

### 9. Connect Anchor
Click the "Connect Anchor" button, you will be prompted to select the serial port.

### 10. Set Distance
Set the distance between the anchor and tag in centimeters.

### 11. Select Anchor Index
Select the corresponding index for the ANCHOR you're calibrating (0-3).

### 12. Monitor and Adjust
Monitor the reading and adjust the Antenna Delay value in the text field or with the Quick Adjustment buttons. These show up as below:
```
range: [830,424,679,999,0,0,0,0]
```
These values are in the order of the TAG IDs (T0, T1, T2, and so on).

### 13. Save Settings
Commit this setting by using the "Set Antenna Delay", "Save Config" and "Restart Module" buttons.

### 14. Wait for Stabilization
The UI will notify you when the average error is within 5% of the real-world distance. (Wait for a moment for the values to stabilize).

### 15. Repeat for All ANCHORs
Repeat steps 5-14 above for ANCHORs A1, A2, and A3 using the same TAG and distance for consistency:

![Multiple ANCHOR Calibration Setup](/images/setupAndCalibration__Page_5_Image_0001.jpg)