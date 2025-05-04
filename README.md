# IoT Environmental Control System with BMP280 & AHT20 Sensors

## 1. Project Overview

### Main Features

#### Environment Monitoring:
- **AHT20** sensor: measures temperature and humidity.
- **BMP280** sensor: measures air pressure and calculates altitude.
- Sensor data is transmitted via **MQTT** and visualized using **Node-RED** or **MQTT Dashboard**.

#### Device Control:
- **Heater** and **Humidifier** are controlled via **relay modules**.

**Automatic Mode:**
- Devices are switched ON/OFF based on pre-set temperature and humidity thresholds.

**Manual Mode:**
- Users can manually toggle devices using **push buttons** or **MQTT commands**.

#### OLED Display:
- Real-time display of **temperature, humidity, pressure, altitude**, and **device status**.

#### Web Configuration:
- If **WiFi or MQTT connection fails**, the ESP32 switches to **Access Point (AP) mode** for reconfiguration via a built-in web interface.

---

## 2. Hardware Wiring

### Component List
- **Microcontroller**: ESP32
- **Sensors**: AHT20 (Temp + Humidity), BMP280 (Pressure + Altitude)
- **Display**: OLED SSD1306 (128x64, I2C)
- **Relays**: 2-channel relay module (for heater and humidifier)
- **Push Buttons**: 3 (Mode, Heater Manual, Humidifier Manual)
- **Wires, Breadboard, Power Supply**

### Pin Mapping

| Component             | ESP32 GPIO Pin     | Description                      |
|----------------------|--------------------|----------------------------------|
| **OLED SSD1306**     | SDA: GPIO21        | I2C shared bus                   |
|                      | SCL: GPIO22        |                                  |
|                      | VCC, GND           | Power (3.3V or 5V)               |
| **AHT20 Sensor**      | SDA: GPIO21        |                                  |
|                      | SCL: GPIO22        |                                  |
| **BMP280 Sensor**     | SDA: GPIO21        |                                  |
|                      | SCL: GPIO22        |                                  |
| **Relay 1 (Heater)**  | IN1: GPIO26        | COM and NO/NC to device          |
| **Relay 2 (Humidifier)** | IN2: GPIO27     |                                  |
| **Mode Button**       | GPIO16             | One leg to GND                   |
| **Heater Button**     | GPIO17             |                                  |
| **Humidifier Button** | GPIO18             |                                  |

### Power Supply
- ESP32: powered via USB or 5V supply.
- Relays and sensors: powered from 3.3V/5V pin of ESP32 or external source (recommended for high-current devices).

---

## 3. Running Node-RED on Raspberry Pi 4

### Install Node-RED

```bash
sudo apt update && sudo apt upgrade -y
curl -sL https://deb.nodesource.com/setup_16.x | sudo -E bash -
sudo apt install -y nodejs
sudo npm install -g --unsafe-perm node-red
Start Node-RED
bash
node-red
Access Node-RED in your browser:
http://<RaspberryPi-IP>:1880
(e.g. http://192.168.1.100:1880)

4. Import Node-RED Flow
Open Node-RED in the browser.

Click the menu (top-right corner) > Import > Clipboard.

Paste the JSON flow below:

[
  {
    "id": "mqtt-in",
    "type": "mqtt in",
    "z": "flow1",
    "name": "Receive Sensor Data",
    "topic": "iot/state/#",
    "qos": "2",
    "datatype": "auto",
    "broker": "mqtt-broker",
    "x": 160,
    "y": 180,
    "wires": [
      ["debug-node"]
    ]
  },
  {
    "id": "mqtt-out",
    "type": "mqtt out",
    "z": "flow1",
    "name": "Control Devices",
    "topic": "iot/control/device1",
    "qos": "2",
    "retain": "false",
    "broker": "mqtt-broker",
    "x": 160,
    "y": 300,
    "wires": []
  },
  {
    "id": "debug-node",
    "type": "debug",
    "z": "flow1",
    "name": "Debug Sensor Data",
    "active": true,
    "console": "true",
    "complete": "true",
    "x": 400,
    "y": 180,
    "wires": []
  },
  {
    "id": "mqtt-broker",
    "type": "mqtt-broker",
    "z": "",
    "name": "MQTT Broker",
    "broker": "192.168.1.100",
    "port": "1883",
    "clientid": "NodeRED_Client",
    "usetls": false,
    "compatmode": true,
    "keepalive": "60",
    "cleansession": true,
    "birthTopic": "",
    "birthQos": "0",
    "birthRetain": "false",
    "birthPayload": "",
    "closeTopic": "",
    "closeQos": "0",
    "closeRetain": "false",
    "closePayload": "",
    "willTopic": "",
    "willQos": "0",
    "willRetain": "false",
    "willPayload": ""
  }
]
⚠️ Replace the broker IP address (192.168.1.100) with your actual MQTT broker IP.

5. MQTT Topics
From ESP32 (Sensor Data):
iot/state/temperature – Current temperature

iot/state/humidity – Current humidity

iot/state/pressure – Current pressure

iot/state/altitude – Calculated altitude

iot/state/mode – Current control mode (AUTO/MANUAL)

iot/state/device1 – Heater status (ON/OFF)

iot/state/device2 – Humidifier status (ON/OFF)

From Node-RED (Control Commands):
iot/control/mode – Change control mode (AUTO/MANUAL)

iot/control/device1 – Turn ON/OFF Heater

iot/control/device2 – Turn ON/OFF Humidifier

iot/control/temp_threshold – Set temperature threshold

iot/control/humi_threshold – Set humidity threshold

