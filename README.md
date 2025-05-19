# IoT Environmental Control System with BMP280/AHT20 Sensors

A comprehensive IoT system for monitoring and controlling environmental conditions using ESP32, BMP280, and AHT20 sensors. The system provides real-time monitoring of temperature, humidity, pressure, and altitude, with automated control capabilities for environmental regulation.

## Features

- **Real-time Environmental Monitoring**
  - Temperature (°C)
  - Humidity (%)
  - Atmospheric Pressure (hPa)
  - Altitude (m)

- **Automated Control System**
  - Automatic mode for environmental regulation
  - Manual control options
  - Timer functionality for scheduled operations
  - Two relay-controlled devices:
    - Fan/Heater for temperature control
    - Humidifier for humidity control

- **Web Interface**
  - Real-time data visualization
  - Interactive control dashboard
  - Historical data charts with time range selection
  - Responsive design for desktop and mobile

- **Hardware Components**
  - ESP32 microcontroller
  - BMP280 sensor (temperature & pressure)
  - AHT20 sensor (temperature & humidity)
  - OLED display for local monitoring
  - Relay modules for device control
  - Power supply unit
  - Raspberry Pi 4 (server)

## System Architecture

1. **Hardware Layer**
   - ESP32 reads sensor data from BMP280 and AHT20
   - Local display on OLED screen
   - Relay control for connected devices

2. **Communication Layer**
   - MQTT protocol for real-time communication
   - WebSocket for web interface updates
   - WiFi connectivity with fallback AP mode

3. **Backend Services (Raspberry Pi 4)**
   - Node-RED for MQTT message handling
   - MySQL database (via phpMyAdmin)
   - Apache web server

4. **Frontend Interface**
   - Real-time monitoring dashboard
   - Interactive control panel
   - Historical data visualization
   - Mobile-responsive design

## Installation & Setup

### Hardware Setup

1. Connect the sensors to ESP32:
   - BMP280: I2C connection (Address: 0x77)
   - AHT20: I2C connection
   - OLED Display: I2C connection (Address: 0x3C)

2. Connect the relay modules:
   - Heater/Fan control: GPIO 18
   - Humidifier control: GPIO 19

3. Connect control buttons:
   - Mode button: GPIO 25
   - Device 1 button: GPIO 26
   - Device 2 button: GPIO 27

### Raspberry Pi 4 Setup

1. **Install Required Software**
   ```bash
   # Update system
   sudo apt update
   sudo apt upgrade

   # Install Apache
   sudo apt install apache2
   sudo systemctl enable apache2
   
   # Install MySQL and phpMyAdmin
   sudo apt install mariadb-server php php-mysql phpmyadmin
   
   # Install Node-RED
   bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
   sudo systemctl enable nodered.service
   ```

2. **Configure Apache and phpMyAdmin**
   ```bash
   # Enable PHP in Apache
   sudo a2enmod php
   
   # Configure phpMyAdmin
   sudo ln -s /usr/share/phpmyadmin /var/www/html/
   sudo systemctl restart apache2
   ```

3. **Database Setup**
   ```bash
   # Access MySQL
   sudo mysql -u root

   # Create database and user
   CREATE DATABASE environment_data;
   CREATE USER 'pi'@'localhost' IDENTIFIED BY 'your_password';
   GRANT ALL PRIVILEGES ON environment_data.* TO 'pi'@'localhost';
   FLUSH PRIVILEGES;
   EXIT;

   # Import database schema
   cd /path/to/project
   mysql -u pi -p environment_data < database/environment_data.sql
   ```

4. **Web Interface Setup**
   ```bash
   # Copy web files
   sudo cp -r Done_project/web/* /var/www/html/
   sudo chown -R www-data:www-data /var/www/html/
   ```

### ESP32 Setup

1. **Install Arduino Libraries**
   ```bash
   # Open Arduino IDE
   # Install required libraries:
   - Adafruit_GFX
   - Adafruit_SSD1306
   - Adafruit_AHTX0
   - Adafruit_BMP280
   - PubSubClient
   - ESPAsyncWebServer
   - AsyncTCP

   # Flash the ESP32 with DACNIII.ino
   ```

## Running the Project

1. **Start Raspberry Pi Services**
   ```bash
   # Check service status
   sudo systemctl status apache2
   sudo systemctl status mariadb
   sudo systemctl status nodered

   # Start services if not running
   sudo systemctl start apache2
   sudo systemctl start mariadb
   sudo systemctl start nodered
   ```

2. **Node-RED Setup**
   ```bash
   # Access Node-RED
   http://<raspberry_pi_ip>:1880
   
   # Import flows.json from node_scrip folder
   # Configure MQTT broker settings
   ```

3. **ESP32 Configuration**
   - Power up the ESP32
   - Connect to ESP32_Config WiFi network (Password: 12345678)
   - Open http://192.168.4.1
   - Configure:
     - WiFi credentials
     - MQTT broker IP (Raspberry Pi IP)

4. **Access Web Interface**
   - Open http://<raspberry_pi_ip>/web/
   - Default database credentials in index.php:
     - Username: pi
     - Password: your_password
     - Database: environment_data

5. **Verify System**
   - Check ESP32 connection status
   - Verify sensor readings
   - Test device controls
   - Monitor data logging

## Usage

1. **Control Modes**
   - **Automatic Mode**: System controls devices based on thresholds
   - **Manual Mode**: Direct control of individual devices
   - **Timer Mode**: Schedule device operations

2. **Data Visualization**
   - Real-time updates every 5 seconds
   - Historical data view (1-72 hours)
   - Zoomable and interactive charts

## Project Structure

```
Done_project/
├── DACNIII/
│   └── DACNIII.ino          # ESP32 firmware
├── database/
│   └── environment_data.sql  # Database schema
├── node_scrip/
│   └── flows.json           # Node-RED flows
└── web/
    └── index.php            # Web interface
```

## Troubleshooting

1. **Raspberry Pi Services**
   - Check service status: `sudo systemctl status <service_name>`
   - View logs: `sudo journalctl -u <service_name>`
   - Verify permissions: `ls -l /var/www/html/`

2. **Database Connection**
   - Test MySQL: `mysql -u pi -p`
   - Check logs: `sudo tail -f /var/log/mysql/error.log`
   - Verify database: `mysql -u pi -p -e "use environment_data; show tables;"`

3. **ESP32 Connection**
   - Verify WiFi connectivity
   - Check MQTT broker address
   - Monitor serial output for debugging

4. **Sensor Reading Errors**
   - Check I2C connections
   - Verify sensor addresses
   - Monitor serial output

## Author

Copyright (c) 2024 [Your Full Name]

## License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
