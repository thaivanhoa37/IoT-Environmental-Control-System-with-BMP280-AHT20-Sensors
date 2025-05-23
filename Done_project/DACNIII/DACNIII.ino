// Using the same code from version2 since it already supports BMP280 and AHT20
// Just removing any remaining DHT11 and AQI sensor references and optimizing the code
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// MQTT Topic Definitions - Exactly matching Node-RED flow
const char* TOPIC_STATE_TEMPERATURE = "esp32/sensor/temperature";
const char* TOPIC_STATE_HUMIDITY = "esp32/sensor/humidity";
const char* TOPIC_STATE_PRESSURE = "esp32/sensor/pressure";
const char* TOPIC_STATE_ALTITUDE = "esp32/sensor/altitude";
const char* TOPIC_STATE_MODE = "esp32/mode/state";
const char* TOPIC_STATE_DEVICE1 = "esp32/heater/state";
const char* TOPIC_STATE_DEVICE2 = "esp32/humidifier/state";
const char* TOPIC_STATE_TEMP_THRESHOLD = "esp32/threshold/temperature";
const char* TOPIC_STATE_HUMI_THRESHOLD = "esp32/threshold/humidity";
const char* TOPIC_CONTROL_MODE = "esp32/mode/set";
const char* TOPIC_CONTROL_DEVICE1 = "esp32/heater/set";
const char* TOPIC_CONTROL_DEVICE2 = "esp32/humidifier/set";
const char* TOPIC_CONTROL_TEMP_THRESHOLD = "esp32/threshold/temperature/set";
const char* TOPIC_CONTROL_HUMI_THRESHOLD = "esp32/threshold/humidity/set";

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensor initialization
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
bool bmpInitialized = false;
bool ahtInitialized = false;
bool oledInitialized = false;

// Pin definitions
#define BUTTON_MODE 25
#define BUTTON_DEVICE1 26
#define BUTTON_DEVICE2 27
#define DEVICE1_PIN 18  // Heater relay control
#define DEVICE2_PIN 19  // Humidifier relay control

// EEPROM Configuration
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

// Server initialization
AsyncWebServer server(80);

// WiFi and MQTT Configuration
char ssid[32] = "vanhoa";
char password[32] = "11111111";
char mqtt_server[32] = "192.168.137.127";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_IOT_Controller";

// State variables
float temperature = 0, humidity = 0, pressure = 0, altitude = 0;
float tempSum = 0, humiSum = 0, pressSum = 0;
int count = 0;
int countTime = 0;

float avgTemperature = 0;
float avgHumidity = 0;
float avgPressure = 0;
float avgAltitude = 0;
bool updateDisplay = false;
bool isAutoMode = false;
bool device1State = false;  // Heater
bool device2State = false;  // Humidifier
bool dataUpdated = false;
bool publishSensorFlag = false;
bool publishDeviceFlag = false;
bool thresholdUpdated = false;
bool isAPMode = false;

float tempThreshold = 35.0;// sét ngưỡng nhiệt độ
float humiThreshold = 70.0; //sét ngưỡng độ ẩm

char msg[50];

// Connection management variables
int failedReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 10;
const unsigned long RESET_TIMEOUT = 300000;  // 5 minutes

WiFiClient espClient;
PubSubClient client(espClient);

// Task handles
TaskHandle_t TaskReadHandle;
TaskHandle_t TaskPrintHandle;
TaskHandle_t TaskDisplayHandle;
TaskHandle_t TaskControlHandle;
TaskHandle_t TaskMQTTHandle;
TaskHandle_t TaskMQTTSubscribeHandle;

// I2C Scanner
void scanI2C() {
  Serial.println("Scanning I2C...");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Found I2C device at address: 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  Serial.println("I2C scan complete.");
}

// OLED Initialization
bool initializeOLED() {
  Serial.print("Initializing OLED at address: 0x");
  Serial.println(SCREEN_ADDRESS, HEX);
  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED initialized successfully!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED Initialized");
    display.display();
    return true;
  }
  Serial.println("❌ Failed to initialize OLED! Check I2C connection.");
  return false;
}

// WiFi and MQTT Management Functions
void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASS_ADDR, password);
  EEPROM.get(MQTT_IP_ADDR, mqtt_server);
  EEPROM.end();
  Serial.println("Loaded credentials from EEPROM:");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("MQTT Server: "); Serial.println(mqtt_server);
}

void saveCredentials(const char* newSsid, const char* newPass, const char* newMqtt) {
  char tempSsid[32], tempPass[32], tempMqtt[32];
  strncpy(tempSsid, newSsid, 32);
  strncpy(tempPass, newPass, 32);
  strncpy(tempMqtt, newMqtt, 32);
  tempSsid[31] = tempPass[31] = tempMqtt[31] = '\0';

  if (strcmp(tempSsid, ssid) == 0 && strcmp(tempPass, password) == 0 && strcmp(tempMqtt, mqtt_server) == 0) {
    Serial.println("No changes in credentials, skipping EEPROM write");
    return;
  }

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(SSID_ADDR, tempSsid);
  EEPROM.put(PASS_ADDR, tempPass);
  EEPROM.put(MQTT_IP_ADDR, tempMqtt);
  EEPROM.commit();
  EEPROM.end();

  strncpy(ssid, tempSsid, 32);
  strncpy(password, tempPass, 32);
  strncpy(mqtt_server, tempMqtt, 32);
  Serial.println("New credentials saved to EEPROM");
}

bool setup_wifi() {
  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to %s\n", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    failedReconnectAttempts = 0;
    return true;
  }
  Serial.println("\nWiFi connection failed");
  return false;
}

// Web Configuration Page
String getConfigPage() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>ESP32 Configuration</title>
      <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .container { background-color: #fff; padding: 30px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 100%; max-width: 400px; }
        h1 { text-align: center; color: #333; }
        input[type="text"], input[type="password"] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 5px; }
        input[type="submit"] { background-color: #28a745; color: white; padding: 10px 15px; border: none; border-radius: 5px; width: 100%; cursor: pointer; }
        input[type="submit"]:hover { background-color: #218838; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>ESP32 Configuration</h1>
        <form method='POST' action='/save'>
          WiFi SSID:<br>
          <input type='text' name='ssid' value='%SSID%'><br>
          Password:<br>
          <input type='password' name='pass' value='%PASS%'><br>
          MQTT Server:<br>
          <input type='text' name='mqtt' value='%MQTT%'><br>
          <input type='submit' value='Save'>
        </form>
      </div>
    </body>
    </html>
  )rawliteral";

  html.replace("%SSID%", String(ssid));
  html.replace("%PASS%", String(password));
  html.replace("%MQTT%", String(mqtt_server));
  return html;
}

// AP Mode Setup
void startAPMode() {
  isAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  Serial.println("AP Mode started");
  Serial.print("AP IP: "); 
  Serial.println(WiFi.softAPIP());

  if (oledInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("AP Config Mode");
    display.println("SSID: ESP32_Config");
    display.println("Pass: 12345678");
    display.println("IP: 192.168.4.1");
    display.display();
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getConfigPage());
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasArg("ssid") && request->hasArg("pass") && request->hasArg("mqtt")) {
      String newSsid = request->arg("ssid");
      String newPass = request->arg("pass");
      String newMqtt = request->arg("mqtt");
      saveCredentials(newSsid.c_str(), newPass.c_str(), newMqtt.c_str());
      request->send(200, "text/html", "<html><body><h1>Saved! Restarting...</h1></body></html>");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    } else {
      request->send(400, "text/html", "<html><body><h1>Invalid data</h1></body></html>");
    }
  });

  server.begin();
}

// MQTT Functions
bool reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting to reconnect...");
    if (!setup_wifi()) {
      failedReconnectAttempts++;
      return false;
    }
  }

  client.disconnect();
  int retryCount = 0;
  while (!client.connected() && retryCount < 5) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      client.subscribe(TOPIC_CONTROL_MODE);
      client.subscribe(TOPIC_CONTROL_DEVICE1);
      client.subscribe(TOPIC_CONTROL_DEVICE2);
      client.subscribe(TOPIC_CONTROL_TEMP_THRESHOLD);
      client.subscribe(TOPIC_CONTROL_HUMI_THRESHOLD);
      failedReconnectAttempts = 0;
      publishDeviceFlag = true;
      return true;
    }
    Serial.printf("failed, rc=%d try again in 2 seconds\n", client.state());
    vTaskDelay(pdMS_TO_TICKS(2000));
    retryCount++;
  }

  failedReconnectAttempts++;
  return false;
}

void publishMQTTSensor() {
  if (!client.connected() && !reconnect()) {
    Serial.println("MQTT disconnected, cannot publish sensor data!");
    return;
  }

  snprintf(msg, 50, "%.1f", avgTemperature);
  client.publish(TOPIC_STATE_TEMPERATURE, msg);
  
  snprintf(msg, 50, "%.1f", avgHumidity);
  client.publish(TOPIC_STATE_HUMIDITY, msg);
  
  snprintf(msg, 50, "%.1f", avgPressure);
  client.publish(TOPIC_STATE_PRESSURE, msg);
  
  snprintf(msg, 50, "%.1f", avgAltitude);
  client.publish(TOPIC_STATE_ALTITUDE, msg);
}

void publishMQTTDevice() {
  if (!client.connected() && !reconnect()) {
    Serial.println("MQTT disconnected, cannot publish device state!");
    return;
  }

  client.publish(TOPIC_STATE_MODE, isAutoMode ? "ON" : "OFF");
  
  bool device1ActualState = !digitalRead(DEVICE1_PIN);
  bool device2ActualState = !digitalRead(DEVICE2_PIN);
  
  client.publish(TOPIC_STATE_DEVICE1, device1ActualState ? "ON" : "OFF");
  client.publish(TOPIC_STATE_DEVICE2, device2ActualState ? "ON" : "OFF");
  
  snprintf(msg, 50, "%.1f", tempThreshold);
  client.publish(TOPIC_STATE_TEMP_THRESHOLD, msg);
  
  snprintf(msg, 50, "%.1f", humiThreshold);
  client.publish(TOPIC_STATE_HUMI_THRESHOLD, msg);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  for (unsigned int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  
  String topicStr = String(topic);
  Serial.printf("Message arrived [%s]: %s\n", topic, messageTemp.c_str());

  if (topicStr == TOPIC_CONTROL_MODE) {
    isAutoMode = (messageTemp == "ON");
    if (!isAutoMode) {
      device1State = device2State = false;
      digitalWrite(DEVICE1_PIN, HIGH);  // Relay logic is inverted
      digitalWrite(DEVICE2_PIN, HIGH);
      publishDeviceFlag = true;
    }
    updateDisplay = true;
    dataUpdated = isAutoMode;
  }
  else if (!isAutoMode) {
    if (topicStr == TOPIC_CONTROL_DEVICE1) {
      device1State = (messageTemp == "ON");
      digitalWrite(DEVICE1_PIN, !device1State);
      updateDisplay = true;
      publishDeviceFlag = true;
    }
    else if (topicStr == TOPIC_CONTROL_DEVICE2) {
      device2State = (messageTemp == "ON");
      digitalWrite(DEVICE2_PIN, !device2State);
      updateDisplay = true;
      publishDeviceFlag = true;
    }
  }
  
  if (topicStr == TOPIC_CONTROL_TEMP_THRESHOLD) {
    float newTemp = messageTemp.toFloat();
    if (newTemp >= 0 && newTemp <= 50) {
      tempThreshold = newTemp;
      thresholdUpdated = true;
      dataUpdated = true;
      publishDeviceFlag = true;
      updateDisplay = true;
    }
  }
  else if (topicStr == TOPIC_CONTROL_HUMI_THRESHOLD) {
    float newHumi = messageTemp.toFloat();
    if (newHumi >= 0 && newHumi <= 100) {
      humiThreshold = newHumi;
      thresholdUpdated = true;
      dataUpdated = true;
      publishDeviceFlag = true;
      updateDisplay = true;
    }
  }
}

// Task Definitions
void TaskReadSensor(void *pvParameters) {
  while (1) {
    sensors_event_t humidity_event, temp_event;
    
    if (ahtInitialized && aht.getEvent(&humidity_event, &temp_event)) {
      temperature = temp_event.temperature;
      humidity = humidity_event.relative_humidity;
    } else {
      temperature = humidity = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(50));  // Small delay between sensor readings

    if (bmpInitialized) {
      pressure = bmp.readPressure() / 100.0;  // Convert Pa to hPa
      altitude = bmp.readAltitude(1013.25);
      
      if (isnan(pressure) || isnan(altitude)) {
        pressure = altitude = 0;
      }
    }

    if (temperature != 0 && humidity != 0) {
      tempSum += temperature;
      humiSum += humidity;
      if (pressure != 0) pressSum += pressure;
      count++;
    }
    countTime++;

    Serial.printf("Temperature: %.1f°C, Humidity: %.1f%%, Pressure: %.1f hPa, Altitude: %.1f m\n",
                 temperature, humidity, pressure, altitude);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void TaskPrintData(void *pvParameters) {
  while (1) {
      if (countTime >= 15) {  // Average over 30 seconds (15 readings at 2-second intervals)
        if (count > 0) {
          avgTemperature = tempSum / count;
          avgHumidity = humiSum / count;
          avgPressure = pressSum / count;
          avgAltitude = bmpInitialized ? bmp.readAltitude(1013.25) : 0;

          Serial.println("\n=== 30-Second Averages ===");
          Serial.printf("Temperature: %.1f°C\n", avgTemperature);
          Serial.printf("Humidity: %.1f%%\n", avgHumidity);
          Serial.printf("Pressure: %.1f hPa\n", avgPressure);
          Serial.printf("Altitude: %.1f m\n", avgAltitude);
          Serial.println("=========================\n");

          updateDisplay = true;
          dataUpdated = isAutoMode;  // Only update control logic in auto mode
          publishSensorFlag = true;
          
          // Only publish device state if in auto mode or manual state changed
          if (isAutoMode || thresholdUpdated) {
            publishDeviceFlag = true;
            thresholdUpdated = false;
          }

          tempSum = humiSum = pressSum = 0;
          count = countTime = 0;
        }
      }
    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}

void TaskDisplay(void *pvParameters) {
  while (1) {
    if (!isAPMode && updateDisplay && oledInitialized) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);

      display.printf("Temp: %.1f C\n", avgTemperature);
      display.printf("Hum: %.1f %%\n", avgHumidity);
      display.printf("Press: %.1f hPa\n", avgPressure);
      display.printf("Alt: %.1f m\n", avgAltitude);
      
      bool dev1 = !digitalRead(DEVICE1_PIN);
      bool dev2 = !digitalRead(DEVICE2_PIN);
      display.printf("Heat: %s\n", dev1 ? "ON" : "OFF");
      display.printf("Hum: %s\n", dev2 ? "ON" : "OFF");
      display.printf("Mode: %s", isAutoMode ? "Auto" : "Manual");
      
      display.display();
      updateDisplay = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void TaskControl(void *pvParameters) {
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_DEVICE1, INPUT_PULLUP);
  pinMode(BUTTON_DEVICE2, INPUT_PULLUP);
  pinMode(DEVICE1_PIN, OUTPUT);
  pinMode(DEVICE2_PIN, OUTPUT);

  digitalWrite(DEVICE1_PIN, HIGH);  // Relay logic is inverted (HIGH = OFF)
  digitalWrite(DEVICE2_PIN, HIGH);

  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 200;

  while (1) {
    unsigned long currentTime = millis();

    // Mode button handling
    if (digitalRead(BUTTON_MODE) == LOW && (currentTime - lastDebounceTime) > debounceDelay) {
      isAutoMode = !isAutoMode;
      if (!isAutoMode) {
        device1State = device2State = false;
        digitalWrite(DEVICE1_PIN, HIGH);
        digitalWrite(DEVICE2_PIN, HIGH);
      }
      updateDisplay = true;
      publishDeviceFlag = true;
      dataUpdated = isAutoMode;
      lastDebounceTime = currentTime;
    }

    // Manual mode device control
    if (!isAutoMode) {
      if (digitalRead(BUTTON_DEVICE1) == LOW && (currentTime - lastDebounceTime) > debounceDelay) {
        device1State = !device1State;
        digitalWrite(DEVICE1_PIN, !device1State);
        updateDisplay = true;
        publishDeviceFlag = true;
        lastDebounceTime = currentTime;
      }

      if (digitalRead(BUTTON_DEVICE2) == LOW && (currentTime - lastDebounceTime) > debounceDelay) {
        device2State = !device2State;
        digitalWrite(DEVICE2_PIN, !device2State);
        updateDisplay = true;
        publishDeviceFlag = true;
        lastDebounceTime = currentTime;
      }
    }

    // Only run automatic control logic when in auto mode and data is updated
    if (isAutoMode && dataUpdated) {
      // Store previous states
      bool prevDev1 = device1State;
      bool prevDev2 = device2State;

      // Update device states based on thresholds
      device1State = (avgTemperature < tempThreshold);
      device2State = (avgHumidity < humiThreshold);

      // Only update outputs and publish if states have changed
      if (prevDev1 != device1State || prevDev2 != device2State) {
        digitalWrite(DEVICE1_PIN, !device1State);
        digitalWrite(DEVICE2_PIN, !device2State);
        updateDisplay = true;
        publishDeviceFlag = true;
      }
      dataUpdated = false;
    } else if (!isAutoMode) {
      // In manual mode, don't update dataUpdated flag
      dataUpdated = false;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void TaskMQTT(void *pvParameters) {
  unsigned long lastReconnectAttempt = 0;
  const unsigned long reconnectInterval = 5000;

  while (1) {
    unsigned long currentMillis = millis();

    if (!isAPMode) {
      if (!client.connected()) {
        if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
          if (reconnect()) {
            lastReconnectAttempt = 0;
          } else {
            lastReconnectAttempt = currentMillis;
            if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
              Serial.println("Too many failed attempts, switching to AP mode");
              startAPMode();
            }
          }
        }
      } else {
        client.loop();
        
        if (publishSensorFlag) {
          publishMQTTSensor();
          publishSensorFlag = false;
        }
        
        if (publishDeviceFlag) {
          publishMQTTDevice();
          publishDeviceFlag = false;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000));

  Wire.begin();
  scanI2C();
  
  oledInitialized = initializeOLED();
  
  int attempts = 0;
  while (!aht.begin() && attempts < 3) {
    Serial.println("Could not find AHT20! Retrying...");
    attempts++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ahtInitialized = aht.begin();
  
  attempts = 0;
  while (!bmp.begin(0x77) && attempts < 3) {
    Serial.println("Could not find BMP280! Retrying...");
    attempts++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  bmpInitialized = bmp.begin(0x77);

  if (oledInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.println("System starting...");
    display.display();
  }

  loadCredentials();
  
  if (setup_wifi()) {
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    if (!reconnect()) {
      Serial.println("Initial MQTT connection failed");
    }
  } else {
    Serial.println("Initial WiFi connection failed");
    startAPMode();
  }

  // Create tasks
  xTaskCreatePinnedToCore(TaskReadSensor, "ReadSensor", 4096, NULL, 3, &TaskReadHandle, 1);
  xTaskCreatePinnedToCore(TaskPrintData, "PrintData", 4096, NULL, 1, &TaskPrintHandle, 1);
  xTaskCreatePinnedToCore(TaskDisplay, "Display", 4096, NULL, 1, &TaskDisplayHandle, 0);
  xTaskCreatePinnedToCore(TaskControl, "Control", 4096, NULL, 2, &TaskControlHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTT, "MQTT", 4096, NULL, 1, &TaskMQTTHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));  // Background task
}
