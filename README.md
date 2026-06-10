# WiFi CSI Fall Detection System

This repository contains the firmware and dashboard for a non-intrusive, real-time fall detection system. By leveraging Channel State Information (CSI) from standard Wi-Fi packets, this system detects the distinct radio frequency (RF) disruptions caused by human falls, entirely eliminating the need for wearables or cameras.

Developed for Stanford University EE284A - Introduction to Internet of Things.

---

## 📡 System Architecture

The project consists of three main components communicating via a classic IoT publish/subscribe architecture: a Transmitter (Tx) node, a Receiver (Rx) node, and a web-based visualisation dashboard.

### 1. Transmitter Node (`CSI_send.ino`)
An ESP32 configured as an ESP-NOW broadcaster.
* **Protocol:** Uses 802.11n (MCS0) rather than the legacy 1Mbps mode to ensure rich CSI data
* **Frequency:** Broadcasts dummy payloads at a rate of ~100 Hz
* **Network:** Connects to a standard mobile hotspot to automatically synchronise to the correct operating channel

### 2. Receiver Node (`csi_recv_2.ino`)
An ESP32 operating in promiscuous mode to capture the channel state of the incoming packets.
* **Subcarrier Extraction:** Drops noisy null and DC subcarriers, extracting $52$ valid subcarriers from the HT20 format (indices 6–31 and 33–58).
* **Amplitude Calculation:** Calculates the raw amplitude from the real and imaginary parts of the signal:
$$A = \sqrt{\text{real}^2 + \text{imag}^2}$$.
* **On-device Filtering:** Applies a 9-tap 1D Infinite Impulse Response (IIR) bandpass filter to smooth the raw amplitude in real-time.
* **MQTT Publishing:** Aggregates the filtered subcarriers and forwards them as a JSON payload to `broker.emqx.io` on the topic `ee284a/csi/data`. The node throttles publishing to roughly 10 packets per second to prevent network saturation.

### 3. Dashboard (`index.html`)
A browser-based UI that processes the MQTT stream and detects falls using Principal Component Analysis (PCA).
* **Buffering:** Maintains a sliding window of 150 packets (with a slide step of 10).
* **Dimensionality Reduction:** Mean-centres the data and applies a 3-component PCA to distil the 52 subcarriers into tracking scores.
* **Variance Thresholding:** Splits the sliding window into an "action" phase (first 75 samples) and an "aftermath" phase (last 75 samples). 
* **Detection Logic:** A fall is flagged if the action variance spikes and the aftermath variance indicates sudden stillness:
  $$V_{action} > 500 \quad \text{and} \quad V_{aftermath} < 300$$

---

## 🛠️ Hardware Requirements

* 2x **ESP32 Microcontrollers** (e.g., Adafruit Feather S2)
* Power supply/batteries for the nodes
* A Wi-Fi network or Mobile Hotspot (for channel synchronisation and MQTT internet access)

---

## 💻 Software & Dependencies

### ESP32 Firmware
* **Framework:** Arduino Core for ESP32 / ESP-IDF.
* **Libraries Required:** * `WiFi.h`
  * `esp_now.h` 
  * `esp_wifi.h`
  * `PubSubClient` (for MQTT on the Rx node)

### Web Dashboard
* **MQTT.js:** For WebSockets connection to the MQTT broker.
* **Chart.js:** For real-time waveform rendering.
* *No build step required. Simply open the HTML file in a modern browser.*

---

## 🚀 Setup Instructions

### Step 1: Flash the Transmitter
1. Open `CSI_send.ino` in the Arduino IDE.
2. Update the `ssid` and `password` variables to match your local hotspot/Wifi.
3. Flash the code to the first ESP32.
4. Open the Serial Monitor (115200 baud) and copy the **Tx MAC address** printed to the console. 

### Step 2: Flash the Receiver
1. Open `csi_recv_2.ino`.
2. Update the `ssid` and `password` to match the same hotspot/Wifi.
3. Paste the Tx MAC address into the `expected_mac` array to lock the receiver to the correct transmitter.
4. Flash the code to the second ESP32. Ensure it successfully connects to the Wi-Fi and EMQX broker.

### Step 3: Run the Dashboard
1. Simply double-click `index.html` to open it in your web browser.
2. Ensure the dashboard connects to the MQTT broker. 
3. Move between the Tx and Rx nodes to watch the real-time CSI movement waveform. Simulate a fall to trigger the detection alert!

### Step 4: Calibration & Threshold Tuning
Because CSI amplitude variance is highly dependent on the physical environment, you will likely need to experiment to reliably detect falls in your specific location.
1. Observe the variance baseline in the dashboard while the room is empty, and again during normal walking.
2. Simulate a fall between the Tx and Rx nodes and note the peak Action variance and the subsequent Aftermath variance.
3. Open `index.html` in a text editor and update the constants at the top of the script based on your environment's physical distance and layout:
   ```javascript
   var SPIKE_THRESHOLD     = 500; // Increase if false positives occur from normal movement
   var STILLNESS_THRESHOLD = 300; // Adjust based on background noise in the aftermath phase
---

## 👨‍💻 Authors
**Nimalan S/O Anbhuarasan** Stanford University
**Siddhartha Parupudi** Stanford University
**Jason Jiang** Stanford University
