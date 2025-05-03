#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Adafruit_AHTX0.h> // Thư viện cho AHT20
#include <Adafruit_BMP280.h> // Thư viện cho BMP280

// Khai báo các topic (loại bỏ dust và AQI)
const char* TOPIC_STATE_TEMPERATURE = "node2/state/temperature";
const char* TOPIC_STATE_HUMIDITY = "node2/state/humidity";
const char* TOPIC_STATE_PRESSURE = "node2/state/pressure";
const char* TOPIC_STATE_MODE = "node2/state/mode";
const char* TOPIC_STATE_DEVICE1 = "node2/state/device1";
const char* TOPIC_STATE_DEVICE2 = "node2/state/device2";
const char* TOPIC_STATE_TEMP_THRESHOLD = "node2/state/temp_threshold";

const char* TOPIC_CONTROL_MODE = "node2/control/mode";
const char* TOPIC_CONTROL_DEVICE1 = "node2/control/device1";
const char* TOPIC_CONTROL_DEVICE2 = "node2/control/device2";
const char* TOPIC_CONTROL_TEMP_THRESHOLD = "node2/control/temp_threshold";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define BUTTON_MODE 16
#define BUTTON_DEVICE1 17
#define BUTTON_DEVICE2 18
#define DEVICE1_PIN 26
#define DEVICE2_PIN 27

#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

// Khởi tạo cảm biến
Adafruit_AHTX0 aht; // AHT20
Adafruit_BMP280 bmp; // BMP280
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AsyncWebServer server(80);

float temperature = 0, humidity = 0, pressure = 0;
float tempSum = 0, humSum = 0, pressureSum = 0;
int count = 0;
int countTime = 0;

TaskHandle_t TaskReadHandle;
TaskHandle_t TaskPrintHandle;
TaskHandle_t TaskDisplayHandle;
TaskHandle_t TaskControlHandle;
TaskHandle_t TaskMQTTHandle;
TaskHandle_t TaskMQTTSubscribeHandle;
TaskHandle_t TaskResetHandle;
TaskHandle_t TaskAPTimeoutHandle;

SemaphoreHandle_t mqttMutex;

float avgTemperature = 0;
float avgHumidity = 0;
float avgPressure = 0;
bool updateDisplay = false;
bool isAutoMode = false;
bool device1State = false;
bool device2State = false;
bool dataUpdated = false;
bool publishSensorFlag = false;
bool publishDeviceFlag = false;
bool thresholdUpdated = false;

bool isAPMode = false;

float tempThreshold = 28.0;

char ssid[32] = "P2408";
char password[32] = "244466666";
char mqtt_server[32] = "http://127.0.0.1/";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_Client2";

char msg[50];

int failedReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 10;
const unsigned long RESET_TIMEOUT = 120000; // 2 phút

WiFiClient espClient;
PubSubClient client(espClient);

bool readButton(int pin) {
  return digitalRead(pin) == LOW;
}

void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASS_ADDR, password);
  EEPROM.get(MQTT_IP_ADDR, mqtt_server);
  EEPROM.end();
}

void saveCredentials(const char* newSsid, const char* newPass, const char* newMqtt) {
  char tempSsid[32];
  char tempPass[32];
  char tempMqtt[32];

  strncpy(tempSsid, newSsid, 32);
  strncpy(tempPass, newPass, 32);
  strncpy(tempMqtt, newMqtt, 32);

  tempSsid[31] = '\0';
  tempPass[31] = '\0';
  tempMqtt[31] = '\0';

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(SSID_ADDR, tempSsid);
  EEPROM.put(PASS_ADDR, tempPass);
  EEPROM.put(MQTT_IP_ADDR, tempMqtt);
  EEPROM.commit();
  EEPROM.end();

  strncpy(ssid, tempSsid, 32);
  strncpy(password, tempPass, 32);
  strncpy(mqtt_server, tempMqtt, 32);
}

bool setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println(WiFi.localIP());
    failedReconnectAttempts = 0;
    return true;
  }
  Serial.println("\nWiFi connection failed");
  return false;
}

String getConfigPage() {
  String html = "<!DOCTYPE html><html><body><h1>ESP32 Config</h1>";
  html += "<form method='POST' action='/save'>";
  html += "WiFi SSID: <input type='text' name='ssid' value='" + String(ssid) + "'><br>";
  html += "Password: <input type='text' name='pass' value='" + String(password) + "'><br>";
  html += "MQTT Server: <input type='text' name='mqtt' value='" + String(mqtt_server) + "'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form></body></html>";
  return html;
}

void startAPMode() {
  isAPMode = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  Serial.println("AP Mode started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("AP Mode Active");
  display.println("Connect to:");
  display.println("SSID: ESP32_Config");
  display.println("Pass: 12345678");
  display.println("IP: 192.168.4.1");
  display.println("Open: 192.168.4.1");
  display.display();
  Serial.println("OLED display initialized for AP Mode");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Received HTTP GET request for config page");
    request->send(200, "text/html", getConfigPage());
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Received HTTP POST request to save config");
    if (request->hasArg("ssid") && request->hasArg("pass") && request->hasArg("mqtt")) {
      String newSsid = request->arg("ssid");
      String newPass = request->arg("pass");
      String newMqtt = request->arg("mqtt");

      saveCredentials(newSsid.c_str(), newPass.c_str(), newMqtt.c_str());
      request->send(200, "text/html", "<html><body><h1>Saved! Restarting...</h1></body></html>");
      vTaskDelay(pdMS_TO_TICKS(1000));
      isAPMode = false;
      ESP.restart();
    } else {
      request->send(400, "text/html", "<html><body><h1>Invalid input</h1></body></html>");
    }
  });

  server.begin();
  Serial.println("Async Web Server started");
  Serial.println("Waiting for client connection or timeout...");

  vTaskSuspend(TaskReadHandle);
  vTaskSuspend(TaskPrintHandle);
  vTaskSuspend(TaskDisplayHandle);
  vTaskSuspend(TaskControlHandle);
  vTaskSuspend(TaskMQTTHandle);
  vTaskSuspend(TaskMQTTSubscribeHandle);
}

void TaskAPTimeout(void *pvParameters) {
  const unsigned long AP_TIMEOUT = 15000; // 15 giây để test nhanh
  unsigned long apStartTime = millis();
  bool clientConnected = false;

  Serial.println("TaskAPTimeout started");

  while (isAPMode) {
    if (WiFi.softAPgetStationNum() > 0) {
      clientConnected = true;
      apStartTime = millis();
      Serial.println("Client connected to AP, resetting timeout");
    }

    unsigned long currentTime = millis();
    if (!clientConnected && (currentTime - apStartTime >= AP_TIMEOUT)) {
      Serial.println("AP Mode timeout: No client connected within 15 seconds, exiting AP Mode");
      isAPMode = false;
      break;
    }

    if ((currentTime - apStartTime) % 5000 == 0) {
      Serial.print("AP Mode active for ");
      Serial.print((currentTime - apStartTime) / 1000);
      Serial.println(" seconds");
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  Serial.println("Stopping AP Mode...");
  server.end();
  WiFi.softAPdisconnect(true);
  Serial.println("AP Mode stopped");

  vTaskResume(TaskReadHandle);
  vTaskResume(TaskPrintHandle);
  vTaskResume(TaskDisplayHandle);
  vTaskResume(TaskControlHandle);
  vTaskResume(TaskMQTTHandle);
  vTaskResume(TaskMQTTSubscribeHandle);

  display.clearDisplay();
  display.display();

  Serial.println("Attempting to reconnect to WiFi/MQTT after exiting AP Mode");
  failedReconnectAttempts = 0;

  vTaskDelete(NULL);
}

void resetWiFiClient() {
  espClient.stop();
  espClient = WiFiClient();
}

bool reconnect() {
  if (isAPMode) {
    Serial.println("reconnect: Skipped, system is in AP mode");
    return false;
  }

  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("reconnect: WiFi not connected, attempting to reconnect...");
      if (!setup_wifi()) {
        failedReconnectAttempts++;
        Serial.print("reconnect: Failed reconnect attempts: ");
        Serial.println(failedReconnectAttempts);
        if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
          Serial.println("reconnect: Too many failed attempts, switching to AP mode");
          xSemaphoreGive(mqttMutex);
          startAPMode();
          xTaskCreatePinnedToCore(TaskAPTimeout, "TaskAPTimeout", 4096, NULL, 2, &TaskAPTimeoutHandle, 1);
        }
        xSemaphoreGive(mqttMutex);
        return false;
      }
      Serial.println("reconnect: WiFi reconnected");
      resetWiFiClient();
    }

    Serial.println("reconnect: Disconnecting MQTT client");
    client.disconnect();
    resetWiFiClient();

    int retryCount = 0;
    while (!client.connected() && retryCount < 3) {
      Serial.print("reconnect: Attempting MQTT connection (attempt ");
      Serial.print(retryCount + 1);
      Serial.println(")...");
      if (client.connect(mqtt_client_id)) {
        Serial.println("reconnect: MQTT connected");
        client.subscribe(TOPIC_CONTROL_MODE);
        client.subscribe(TOPIC_CONTROL_DEVICE1);
        client.subscribe(TOPIC_CONTROL_DEVICE2);
        client.subscribe(TOPIC_CONTROL_TEMP_THRESHOLD);
        failedReconnectAttempts = 0;
        xSemaphoreGive(mqttMutex);
        return true;
      } else {
        Serial.print("reconnect: MQTT connection failed, rc=");
        Serial.print(client.state());
        Serial.println(client.state() == -2 ? " (Broker unreachable)" : "");
        Serial.println(" try again in 2 seconds");
        vTaskDelay(pdMS_TO_TICKS(2000));
        retryCount++;
      }
    }

    failedReconnectAttempts++;
    Serial.print("reconnect: Failed reconnect attempts: ");
    Serial.println(failedReconnectAttempts);
    if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
      Serial.println("reconnect: Too many failed attempts, switching to AP mode");
      xSemaphoreGive(mqttMutex);
      startAPMode();
      xTaskCreatePinnedToCore(TaskAPTimeout, "TaskAPTimeout", 4096, NULL, 2, &TaskAPTimeoutHandle, 1);
    }
    xSemaphoreGive(mqttMutex);
    return false;
  }
  Serial.println("reconnect: Failed to acquire mutex");
  return false;
}

void publishMQTTSensor() {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (!client.connected()) {
      if (!reconnect()) {
        xSemaphoreGive(mqttMutex);
        return;
      }
    }
    client.loop();

    snprintf(msg, 50, "%.1f", avgTemperature);
    client.publish(TOPIC_STATE_TEMPERATURE, msg);
    snprintf(msg, 50, "%.1f", avgHumidity);
    client.publish(TOPIC_STATE_HUMIDITY, msg);
    snprintf(msg, 50, "%.1f", avgPressure);
    client.publish(TOPIC_STATE_PRESSURE, msg);

    Serial.println("MQTT sensor data published");
    xSemaphoreGive(mqttMutex);
  }
}

void publishMQTTDevice() {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (!client.connected()) {
      if (!reconnect()) {
        xSemaphoreGive(mqttMutex);
        return;
      }
    }
    client.loop();

    client.publish(TOPIC_STATE_MODE, isAutoMode ? "ON" : "OFF");
    client.publish(TOPIC_STATE_DEVICE1, device1State ? "ON" : "OFF");
    client.publish(TOPIC_STATE_DEVICE2, device2State ? "ON" : "OFF");
    snprintf(msg, 50, "%.1f", tempThreshold);
    client.publish(TOPIC_STATE_TEMP_THRESHOLD, msg);

    Serial.println("MQTT device state published");
    xSemaphoreGive(mqttMutex);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  if (topicStr == TOPIC_CONTROL_MODE) {
    isAutoMode = (messageTemp == "ON");
    Serial.print("Mode updated via MQTT to: "); Serial.println(isAutoMode ? "Auto" : "Manual");
    if (!isAutoMode) {
      device1State = false;
      device2State = false;
      digitalWrite(DEVICE1_PIN, device1State);
      digitalWrite(DEVICE2_PIN, device2State);
      publishDeviceFlag = true;
    }
    updateDisplay = true;
    if (isAutoMode) {
      dataUpdated = true;
    }
  }
  else if (topicStr == TOPIC_CONTROL_DEVICE1 && !isAutoMode) {
    device1State = (messageTemp == "ON");
    digitalWrite(DEVICE1_PIN, device1State);
    Serial.print("Device 1 updated via MQTT to: "); Serial.println(device1State ? "ON" : "OFF");
    updateDisplay = true;
    publishDeviceFlag = true;
  }
  else if (topicStr == TOPIC_CONTROL_DEVICE2 && !isAutoMode) {
    device2State = (messageTemp == "ON");
    digitalWrite(DEVICE2_PIN, device2State);
    Serial.print("Device 2 updated via MQTT to: "); Serial.println(device2State ? "ON" : "OFF");
    updateDisplay = true;
    publishDeviceFlag = true;
  }
  else if (topicStr == TOPIC_CONTROL_TEMP_THRESHOLD) {
    tempThreshold = messageTemp.toFloat();
    Serial.print("Temperature Threshold updated via MQTT to: "); Serial.println(tempThreshold);
    thresholdUpdated = true;
    dataUpdated = true;
  }
}

void TaskReadSensor(void *pvParameters) {
  while (1) {
    // Đọc từ AHT20
    sensors_event_t humidity_event, temp_event;
    if (aht.getEvent(&humidity_event, &temp_event)) {
      temperature = temp_event.temperature;
      humidity = humidity_event.relative_humidity;
    } else {
      Serial.println("❌ Lỗi đọc cảm biến AHT20!");
      temperature = NAN;
      humidity = NAN;
    }

    // Đọc từ BMP280
    if (bmp.takeForcedMeasurement()) {
      float bmp_temp = bmp.readTemperature();
      pressure = bmp.readPressure() / 100.0F;
      if (!isnan(temperature)) {
        temperature = (temperature + bmp_temp) / 2.0;
      } else {
        temperature = bmp_temp;
      }
    } else {
      Serial.println("❌ Lỗi đọc cảm biến BMP280!");
      pressure = NAN;
    }

    if (isnan(temperature) || isnan(humidity) || isnan(pressure)) {
      Serial.println("❌ Lỗi đọc cảm biến!");
    } else {
      tempSum += temperature;
      humSum += humidity;
      pressureSum += pressure;
      count++;
    }
    countTime++;

    Serial.print("Giá trị đo được: ");
    Serial.print("🌡 Nhiệt độ: ");
    Serial.print(temperature);
    Serial.print("°C | 💧 Độ ẩm: ");
    Serial.print(humidity);
    Serial.print("% | 📏 Áp suất: ");
    Serial.print(pressure);
    Serial.println(" hPa");
    Serial.println("-------------------------");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void TaskPrintData(void *pvParameters) {
  while (1) {
    if (countTime >= 150) {
      if (count > 0) {
        avgTemperature = tempSum / count;
        avgHumidity = humSum / count;
        avgPressure = pressureSum / count;

        Serial.println("--------------");
        Serial.println("Trung bình 30s:");
        Serial.print("Trung bình nhiệt độ: ");
        Serial.print(avgTemperature);
        Serial.print("°C | Độ ẩm trung bình: ");
        Serial.print(avgHumidity);
        Serial.print("% | Áp suất trung bình: ");
        Serial.print(avgPressure);
        Serial.println(" hPa");
        Serial.println("--------------");

        updateDisplay = true;
        dataUpdated = true;
        publishSensorFlag = true;
        publishDeviceFlag = true;

        tempSum = 0;
        humSum = 0;
        pressureSum = 0;
        count = 0;
        countTime = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}

void TaskDisplay(void *pvParameters) {
  while (1) {
    if (!isAPMode && updateDisplay) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);

      display.print("Mode: "); display.println(isAutoMode ? "Auto" : "Manual");
      display.print("Temp: "); display.print(avgTemperature, 1); display.println(" C");
      display.print("Hum: "); display.print(avgHumidity, 1); display.println(" %");
      display.print("Pres: "); display.print(avgPressure, 1); display.println(" hPa");
      display.print("Dev1: "); display.println(device1State ? "ON" : "OFF");
      display.print("Dev2: "); display.println(device2State ? "ON" : "OFF");

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

  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 200;

  while (1) {
    unsigned long currentTime = millis();

    if (readButton(BUTTON_MODE) && (currentTime - lastDebounceTime) > debounceDelay) {
      isAutoMode = !isAutoMode;
      Serial.print("Mode switched to: "); Serial.println(isAutoMode ? "Auto" : "Manual");
      
      publishDeviceFlag = true;
      updateDisplay = true;

      if (isAutoMode) {
        bool prevDevice1 = device1State;
        bool prevDevice2 = device2State;

        device1State = (avgTemperature > tempThreshold);
        device2State = (avgTemperature > tempThreshold); // Device2 cũng dựa vào nhiệt độ

        digitalWrite(DEVICE1_PIN, device1State);
        digitalWrite(DEVICE2_PIN, device2State);

        if (prevDevice1 != device1State || prevDevice2 != device2State) {
          Serial.print("Auto Mode - Temp: "); Serial.print(avgTemperature);
          Serial.print("C");
          Serial.print("Device 1: "); Serial.print(device1State ? "ON" : "OFF");
          Serial.print(", Device 2: "); Serial.println(device2State ? "ON" : "OFF");
        }
      } else {
        device1State = false;
        device2State = false;
        digitalWrite(DEVICE1_PIN, device1State);
        digitalWrite(DEVICE2_PIN, device2State);
      }

      lastDebounceTime = currentTime;
    }

    if (!isAutoMode) {
      if (readButton(BUTTON_DEVICE1) && (currentTime - lastDebounceTime) > debounceDelay) {
        device1State = !device1State;
        digitalWrite(DEVICE1_PIN, device1State);
        Serial.print("Device 1 switched to: "); Serial.println(device1State ? "ON" : "OFF");
        updateDisplay = true;
        publishDeviceFlag = true;
        lastDebounceTime = currentTime;
      }

      if (readButton(BUTTON_DEVICE2) && (currentTime - lastDebounceTime) > debounceDelay) {
        device2State = !device2State;
        digitalWrite(DEVICE2_PIN, device2State);
        Serial.print("Device 2 switched to: "); Serial.println(device2State ? "ON" : "OFF");
        updateDisplay = true;
        publishDeviceFlag = true;
        lastDebounceTime = currentTime;
      }
    } else {
      bool prevDevice1 = device1State;
      bool prevDevice2 = device2State;

      device1State = (avgTemperature > tempThreshold);
      device2State = (avgTemperature > tempThreshold);

      if (prevDevice1 != device1State || prevDevice2 != device2State) {
        digitalWrite(DEVICE1_PIN, device1State);
        digitalWrite(DEVICE2_PIN, device2State);

        Serial.print("Auto Mode - Temp: "); Serial.print(avgTemperature);
        Serial.print("C");
        Serial.print("Device 1: "); Serial.print(device1State ? "ON" : "OFF");
        Serial.print(", Device 2: "); Serial.println(device2State ? "ON" : "OFF");
        publishDeviceFlag = true;
        updateDisplay = true;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void TaskMQTT(void *pvParameters) {
  unsigned long lastReconnectAttempt = 0;
  const unsigned long reconnectInterval = 2000;

  while (1) {
    if (isAPMode) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    unsigned long currentMillis = millis();

    if (WiFi.status() != WL_CONNECTED || !client.connected()) {
      Serial.println("Connection lost, attempting to reconnect...");
      if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
        if (!reconnect()) {
          Serial.println("Reconnection failed");
          if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            Serial.println("Too many failed attempts, switching to AP mode");
            startAPMode();
            xTaskCreatePinnedToCore(TaskAPTimeout, "TaskAPTimeout", 4096, NULL, 2, &TaskAPTimeoutHandle, 1);
          }
        }
        lastReconnectAttempt = currentMillis;
      }
    } else {
      lastReconnectAttempt = currentMillis;
    }

    if (publishSensorFlag) {
      publishMQTTSensor();
      publishSensorFlag = false;
    }

    if (publishDeviceFlag) {
      publishMQTTDevice();
      publishDeviceFlag = false;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void TaskResetTimeout(void *pvParameters) {
  unsigned long connectionFailureStart = 0;
  bool connectionLost = false;
  unsigned long apModeStart = 0;
  bool inAPMode = false;

  while (1) {
    Serial.println("TaskResetTimeout running");
    if (isAPMode) {
      if (!inAPMode) {
        inAPMode = true;
        apModeStart = millis();
        Serial.println("Entered AP Mode, starting reset timeout countdown...");
      }
      if (millis() - apModeStart >= RESET_TIMEOUT) {
        Serial.println("AP Mode reset timeout exceeded, resetting ESP32...");
        ESP.restart();
      }
    } else {
      if (inAPMode) {
        inAPMode = false;
        Serial.println("Exited AP Mode, resetting AP timeout...");
      }

      if (WiFi.status() != WL_CONNECTED || !client.connected()) {
        if (!connectionLost) {
          connectionLost = true;
          connectionFailureStart = millis();
          Serial.println("WiFi/MQTT connection lost detected, starting reset timeout countdown...");
        }
        if (millis() - connectionFailureStart >= RESET_TIMEOUT) {
          Serial.println("WiFi/MQTT connection timeout, resetting ESP32...");
          ESP.restart();
        }
      } else {
        if (connectionLost) {
          Serial.println("WiFi/MQTT connection restored, resetting timeout...");
          connectionLost = false;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void TaskMQTTSubscribe(void *pvParameters) {
  client.setCallback(callback);

  while (1) {
    if (isAPMode) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (!client.connected()) {
        if (!reconnect()) {
          xSemaphoreGive(mqttMutex);
          vTaskDelay(pdMS_TO_TICKS(5000));
          continue;
        }
      }
      client.loop();
      xSemaphoreGive(mqttMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin();
  if (!aht.begin()) {
    Serial.println("❌ Không tìm thấy AHT20!");
    while (1);
  }
  if (!bmp.begin()) {
    Serial.println("❌ Không tìm thấy BMP280!");
    while (1);
  }
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);
  Serial.println("AHT20 và BMP280 khởi tạo thành công");

  mqttMutex = xSemaphoreCreateMutex();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Khoi dong...");
  display.display();
  vTaskDelay(pdMS_TO_TICKS(2000));

  loadCredentials();

  bool connectionSuccess = false;
  while (!connectionSuccess) {
    Serial.println("Attempting WiFi connection...");
    if (setup_wifi()) {
      Serial.println("Setting MQTT server...");
      client.setServer(mqtt_server, mqtt_port);
      if (reconnect()) {
        connectionSuccess = true;
      } else {
        Serial.println("MQTT connection failed, starting AP mode");
        WiFi.disconnect();
        startAPMode();
        xTaskCreatePinnedToCore(TaskAPTimeout, "TaskAPTimeout", 4096, NULL, 2, &TaskAPTimeoutHandle, 1);
      }
    } else {
      Serial.println("WiFi connection failed, starting AP mode");
      startAPMode();
      xTaskCreatePinnedToCore(TaskAPTimeout, "TaskAPTimeout", 4096, NULL, 2, &TaskAPTimeoutHandle, 1);
    }
  }

  xTaskCreatePinnedToCore(TaskReadSensor, "TaskRead", 4096, NULL, 3, &TaskReadHandle, 1);
  xTaskCreatePinnedToCore(TaskPrintData, "TaskPrint", 4096, NULL, 1, &TaskPrintHandle, 1);
  xTaskCreatePinnedToCore(TaskDisplay, "TaskDisplay", 4096, NULL, 1, &TaskDisplayHandle, 0);
  xTaskCreatePinnedToCore(TaskControl, "TaskControl", 4096, NULL, 1, &TaskControlHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTT, "TaskMQTT", 8192, NULL, 3, &TaskMQTTHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTTSubscribe, "TaskMQTTSubscribe", 8192, NULL, 2, &TaskMQTTSubscribeHandle, 1);
  xTaskCreatePinnedToCore(TaskResetTimeout, "TaskReset", 4096, NULL, 2, &TaskResetHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}