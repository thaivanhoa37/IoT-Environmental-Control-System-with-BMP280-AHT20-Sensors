# IoT Environmental Control System with BMP280/AHT20 Sensors

A comprehensive IoT system for monitoring and controlling environmental conditions using ESP32, BMP280, and AHT20 sensors. The system provides real-time monitoring of temperature, humidity, pressure, and altitude, with automated control capabilities for environmental regulation.

## Features

- **Real-time Environmental Monitoring**
  - Temperature monitoring (0-50°C)
  - Humidity monitoring (0-100%)
  - Atmospheric Pressure (900-1100 hPa)
  - Altitude measurement

- **Automated Control System**
  - Automatic/Manual mode switching
  - Smart threshold-based control
  - Timer scheduling for devices
  - Relay control system:
    - Temperature: Fan/Heater
    - Humidity: Humidifier

- **Web Interface**
  - Vietnamese language support
  - Real-time data visualization
  - Interactive charts with zoom/pan
  - Time range selection (1-72 hours)
  - Device timer scheduling
  - Connection status monitoring
  - Responsive Bootstrap design

- **Hardware Components**
  - ESP32 microcontroller
  - BMP280 sensor (I2C: 0x77)
  - AHT20 sensor (I2C default)
  - OLED display (I2C: 0x3C)
  - 2x Relay modules
  - Raspberry Pi 4 server

## Project Structure

```
IoT-Environmental-Control-System/
├── Done_project/
│   ├── DACNIII/
│   │   └── DACNIII.ino          # ESP32 main code
│   ├── database/
│   │   ├── create_database.sql  # Database schema
│   │   └── environment_data.sql # Initial data
│   ├── node_scrip/
│   │   └── flows.json          # Node-RED flows
│   └── web/
│       └── index.php           # Web interface
└── README.md
```
![Project Image](https://raw.githubusercontent.com/thaivanhoa37/IoT-Environmental-Control-System-with-BMP280-AHT20-Sensors/refs/heads/main/Freertos%2Bimage/webserver%20(3).png)

## Running Instructions

1. **Prerequisites Install**
   ```bash
   # Raspberry Pi Setup
   sudo apt update && sudo apt upgrade
   sudo apt install apache2 mariadb-server php php-mysql phpmyadmin
   bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
   ```

2. **Project Deploy**
   ```bash
   # Clone project and setup
   cd /var/www/html/
   git clone [project-url]
   cd IoT-Environmental-Control-System/

   # Database setup
   mysql -u root -p1 < Done_project/database/create_database.sql

   # Copy web files
   sudo cp -r Done_project/web/* /var/www/html/
   sudo chown -R www-data:www-data /var/www/html/

   # Import Node-RED flow
   # Access http://localhost:1880
   # Import Done_project/node_scrip/flows.json
   ```

3. **ESP32 Setup**
   ```bash
   # Open Arduino IDE
   # Open Done_project/DACNIII/DACNIII.ino
   # Install libraries and flash
   ```

4. **Access System**
   - Web Interface: http://localhost/
   - Node-RED: http://localhost:1880
   - Default credentials:
     * Database: root/1
     * WiFi: vanhoa/11111111
     * AP Mode: ESP32_Config/12345678

## System Architecture

1. **Hardware Layer**
   - ESP32 with sensors (BMP280, AHT20)
   - OLED display for local monitoring
   - Relay modules for device control

2. **Communication**
   - MQTT broker on Raspberry Pi
   - WebSocket for real-time updates
   - WiFi with AP mode fallback

3. **Backend Services**
   - Node-RED: MQTT handling
   - MySQL: Data storage
   - Apache: Web server

4. **Frontend**
   - Bootstrap-based UI
   - Chart.js visualizations
   - Vietnamese interface

## Hardware Connections

1. **Sensor Connections**
   - BMP280: I2C (0x77)
   - AHT20: I2C (default)
   - OLED: I2C (0x3C)

2. **Control Pins**
   - Heater/Fan: GPIO 18
   - Humidifier: GPIO 19
   - Mode Button: GPIO 25
   - Device 1 Button: GPIO 26
   - Device 2 Button: GPIO 27
![Project Image](https://raw.githubusercontent.com/thaivanhoa37/IoT-Environmental-Control-System-with-BMP280-AHT20-Sensors/refs/heads/main/Freertos%2Bimage/sp%20(1).jpg)
## Libraries Required

- Adafruit_GFX
- Adafruit_SSD1306
- Adafruit_AHTX0
- Adafruit_BMP280
- PubSubClient
- ESPAsyncWebServer
- AsyncTCP

## Troubleshooting

1. **WiFi Configuration**
   - If WiFi connection fails, system enters AP mode
   - Connect to `ESP32_Config` network (password: `12345678`)
   - Access configuration page at `http://192.168.4.1`
   - Enter new WiFi and MQTT settings

2. **MQTT Connection**
   - Verify MQTT broker is running on `192.168.137.241:1883`
   - Check MQTT topics match the expected format
   - Monitor Node-RED debug tab for connection issues

3. **Sensor Readings**
   - Verify I2C connections and addresses
    * BMP280: 0x77
    * AHT20: Default address
    * OLED: 0x3C
   - Check serial output for sensor initialization status

## Author

Created by [thaivanhoa] - [thaivanhoa2002@gmail.com]

## License

Copyright (c) 2024 [thaivanhoa37]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
