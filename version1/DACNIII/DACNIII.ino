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

// Khai báo các topic dưới dạng hằng số
const char* TOPIC_STATE_TEMPERATURE = "iot/state/temperature";
const char* TOPIC_STATE_HUMIDITY = "iot/state/humidity";
const char* TOPIC_STATE_PRESSURE = "iot/state/pressure";
const char* TOPIC_STATE_ALTITUDE = "iot/state/altitude";
const char* TOPIC_STATE_MODE = "iot/state/mode";
const char* TOPIC_STATE_DEVICE1 = "iot/state/device1";
const char* TOPIC_STATE_DEVICE2 = "iot/state/device2";
const char* TOPIC_STATE_TEMP_THRESHOLD = "iot/state/temp_threshold";
const char* TOPIC_STATE_HUMI_THRESHOLD = "iot/state/humi_threshold";

const char* TOPIC_CONTROL_MODE = "iot/control/mode";
const char* TOPIC_CONTROL_DEVICE1 = "iot/control/device1";
const char* TOPIC_CONTROL_DEVICE2 = "iot/control/device2";
const char* TOPIC_CONTROL_TEMP_THRESHOLD = "iot/control/temp_threshold";
const char* TOPIC_CONTROL_HUMI_THRESHOLD = "iot/control/humi_threshold";

// Cấu hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C // Đã xác nhận từ log quét I2C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Khởi tạo cảm biến
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
bool bmpInitialized = false;
bool ahtInitialized = false;
bool oledInitialized = false;

// Định nghĩa chân
#define BUTTON_MODE 25
#define BUTTON_DEVICE1 26
#define BUTTON_DEVICE2 27
#define DEVICE1_PIN 18 // Relay điều khiển lò sưởi (heater)
#define DEVICE2_PIN 19 // Relay điều khiển máy tạo ẩm (humidifier)

// Cấu hình EEPROM
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

// Khởi tạo server
AsyncWebServer server(80);

// Thông tin WiFi và MQTT
char ssid[32] = "vanhoa";
char password[32] = "11111111";
char mqtt_server[32] = "192.168.137.241"; // Cập nhật địa chỉ MQTT broker theo Node-RED
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_IOT_Controller";

// Biến trạng thái
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
bool device1State = false; // Heater
bool device2State = false; // Humidifier
bool dataUpdated = false;
bool publishSensorFlag = false;
bool publishDeviceFlag = false;
bool thresholdUpdated = false;
bool isAPMode = false;

float tempThreshold = 25.0;
float humiThreshold = 60.0;

char msg[50];

// Biến để đếm số lần thử kết nối thất bại
int failedReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 10; // Sau 10 lần thất bại, vào AP mode
const unsigned long RESET_TIMEOUT = 300000; // 5 phút, nếu không kết nối được thì reset

WiFiClient espClient;
PubSubClient client(espClient);

// Xử lý tác vụ
TaskHandle_t TaskReadHandle;
TaskHandle_t TaskPrintHandle;
TaskHandle_t TaskDisplayHandle;
TaskHandle_t TaskControlHandle;
TaskHandle_t TaskMQTTHandle;
TaskHandle_t TaskMQTTSubscribeHandle;

// Hàm quét địa chỉ I2C
void scanI2C() {
  Serial.println("Quét địa chỉ I2C...");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Tìm thấy thiết bị I2C tại địa chỉ: 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  Serial.println("Quét I2C hoàn tất.");
}

// Hàm khởi tạo OLED
bool initializeOLED() {
  Serial.print("Khởi tạo OLED tại địa chỉ: 0x");
  Serial.println(SCREEN_ADDRESS, HEX);
  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Đã khởi tạo OLED thành công!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED Initialized");
    display.display();
    return true;
  }
  Serial.println("❌ Không thể khởi tạo OLED ở địa chỉ đã chọn! Kiểm tra kết nối I2C.");
  return false;
}

// Hàm tải thông tin WiFi và MQTT từ EEPROM
void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASS_ADDR, password);
  EEPROM.get(MQTT_IP_ADDR, mqtt_server);
  EEPROM.end();
  Serial.println("Đã tải thông tin từ EEPROM:");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("MQTT Server: "); Serial.println(mqtt_server);
}

// Hàm lưu thông tin WiFi và MQTT vào EEPROM
void saveCredentials(const char* newSsid, const char* newPass, const char* newMqtt) {
  char tempSsid[32], tempPass[32], tempMqtt[32];
  strncpy(tempSsid, newSsid, 32);
  strncpy(tempPass, newPass, 32);
  strncpy(tempMqtt, newMqtt, 32);
  tempSsid[31] = '\0';
  tempPass[31] = '\0';
  tempMqtt[31] = '\0';

  if (strcmp(tempSsid, ssid) == 0 && strcmp(tempPass, password) == 0 && strcmp(tempMqtt, mqtt_server) == 0) {
    Serial.println("Thông tin không thay đổi, bỏ qua ghi EEPROM");
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
  Serial.println("Đã lưu thông tin mới vào EEPROM:");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("MQTT Server: "); Serial.println(mqtt_server);
}

// Hàm kết nối WiFi
bool setup_wifi() {
  WiFi.mode(WIFI_STA);
  Serial.print("Đang kết nối đến "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  const int maxAttempts = 20;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nĐã kết nối WiFi");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    failedReconnectAttempts = 0;
    return true;
  }
  Serial.println("\nKết nối WiFi thất bại");
  return false;
}

// Hàm tạo trang web cấu hình
String getConfigPage() {
  String html = "<!DOCTYPE html><html><body><h1>Cấu hình ESP32</h1>";
  html += "<form method='POST' action='/save'>";
  html += "WiFi SSID: <input type='text' name='ssid' value='" + String(ssid) + "'><br>";
  html += "Mật khẩu: <input type='text' name='pass' value='" + String(password) + "'><br>";
  html += "MQTT Server: <input type='text' name='mqtt' value='" + String(mqtt_server) + "'><br>";
  html += "<input type='submit' value='Lưu'>";
  html += "</form></body></html>";
  return html;
}

// Hàm khởi động chế độ AP
void startAPMode() {
  isAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  Serial.println("Đã khởi động chế độ AP");
  Serial.print("IP AP: "); Serial.println(WiFi.softAPIP());

  if (oledInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("AP Mode");
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
      request->send(200, "text/html", "<html><body><h1>Đã lưu! Đang khởi động lại...</h1></body></html>");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    } else {
      request->send(400, "text/html", "<html><body><h1>Dữ liệu không hợp lệ</h1></body></html>");
    }
  });

  server.begin();
  Serial.println("Async Web Server started");
}

// Hàm kết nối lại MQTT
bool reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting to reconnect...");
    if (!setup_wifi()) {
      failedReconnectAttempts++;
      Serial.print("WiFi reconnect failed, attempts: ");
      Serial.println(failedReconnectAttempts);
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
      // Gửi trạng thái hiện tại ngay khi kết nối
      publishDeviceFlag = true;
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      vTaskDelay(pdMS_TO_TICKS(2000));
      retryCount++;
    }
  }

  failedReconnectAttempts++;
  Serial.print("MQTT reconnect failed, attempts: ");
  Serial.println(failedReconnectAttempts);
  return false;
}

// Hàm gửi dữ liệu cảm biến qua MQTT
void publishMQTTSensor() {
  if (!client.connected()) {
    if (!reconnect()) {
      Serial.println("MQTT không kết nối, không thể gửi dữ liệu cảm biến!");
      return;
    }
  }
  client.loop();

  snprintf(msg, 50, "%.1f", avgTemperature);
  client.publish(TOPIC_STATE_TEMPERATURE, msg);
  Serial.print("Đã gửi: "); Serial.print(msg); Serial.println(" đến topic iot/state/temperature");
  
  snprintf(msg, 50, "%.1f", avgHumidity);
  client.publish(TOPIC_STATE_HUMIDITY, msg);
  Serial.print("Đã gửi: "); Serial.print(msg); Serial.println(" đến topic iot/state/humidity");
  
  snprintf(msg, 50, "%.1f", avgPressure);
  client.publish(TOPIC_STATE_PRESSURE, msg);
  Serial.print("Đã gửi: "); Serial.print(msg); Serial.println(" đến topic iot/state/pressure");
  
  snprintf(msg, 50, "%.1f", avgAltitude);
  client.publish(TOPIC_STATE_ALTITUDE, msg);
  Serial.print("Đã gửi: "); Serial.print(msg); Serial.println(" đến topic iot/state/altitude");

  Serial.println("MQTT sensor data published");
}

// Hàm gửi trạng thái thiết bị qua MQTT
void publishMQTTDevice() {
  if (!client.connected()) {
    if (!reconnect()) {
      Serial.println("MQTT không kết nối, không thể gửi trạng thái thiết bị!");
      return;
    }
  }
  client.loop();

  client.publish(TOPIC_STATE_MODE, isAutoMode ? "ON" : "OFF");
  Serial.print("Đã gửi: "); Serial.print(isAutoMode ? "ON" : "OFF"); Serial.println(" đến topic iot/state/mode");
  
  client.publish(TOPIC_STATE_DEVICE1, device1State ? "ON" : "OFF");
  Serial.print("Đã gửi: "); Serial.print(device1State ? "ON" : "OFF"); Serial.println(" đến topic iot/state/device1");
  
  client.publish(TOPIC_STATE_DEVICE2, device2State ? "ON" : "OFF");
  Serial.print("Đã gửi: "); Serial.print(device2State ? "ON" : "OFF"); Serial.println(" đến topic iot/state/device2");
  
  snprintf(msg, 50, "%.1f", tempThreshold);
  client.publish(TOPIC_STATE_TEMP_THRESHOLD, msg);
  Serial.print("Đã gửi: "); Serial.print(msg); Serial.println(" đến topic iot/state/temp_threshold");
  
  snprintf(msg, 50, "%.1f", humiThreshold);
  client.publish(TOPIC_STATE_HUMI_THRESHOLD, msg);
  Serial.print("Đã gửi: "); Serial.print(msg); Serial.println(" đến topic iot/state/humi_threshold");

  Serial.println("MQTT device state published");
}

// Hàm callback xử lý dữ liệu từ MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String messageTemp;

  for (unsigned int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  Serial.print("Nhận tin MQTT trên topic: "); Serial.println(topicStr);
  Serial.print("Nội dung: "); Serial.println(messageTemp);

  if (topicStr == TOPIC_CONTROL_MODE) {
    isAutoMode = (messageTemp == "ON");
    Serial.print("Mode updated via MQTT to: "); Serial.println(isAutoMode ? "Auto" : "Manual");
    if (!isAutoMode) {
      device1State = false;
      device2State = false;
      digitalWrite(DEVICE1_PIN, !device1State); // Relay đảo logic
      digitalWrite(DEVICE2_PIN, !device2State);
      publishDeviceFlag = true;
    }
    updateDisplay = true;
    if (isAutoMode) {
      dataUpdated = true;
    }
  }
  else if (topicStr == TOPIC_CONTROL_DEVICE1 && !isAutoMode) {
    device1State = (messageTemp == "ON");
    digitalWrite(DEVICE1_PIN, !device1State);
    Serial.print("Device 1 updated via MQTT to: "); Serial.println(device1State ? "ON" : "OFF");
    updateDisplay = true;
    publishDeviceFlag = true;
  }
  else if (topicStr == TOPIC_CONTROL_DEVICE2 && !isAutoMode) {
    device2State = (messageTemp == "ON");
    digitalWrite(DEVICE2_PIN, !device2State);
    Serial.print("Device 2 updated via MQTT to: "); Serial.println(device2State ? "ON" : "OFF");
    updateDisplay = true;
    publishDeviceFlag = true;
  }
  else if (topicStr == TOPIC_CONTROL_TEMP_THRESHOLD) {
    float newThreshold = messageTemp.toFloat();
    if (newThreshold >= 0 && newThreshold <= 50) { // Giới hạn ngưỡng hợp lý
      tempThreshold = newThreshold;
      Serial.print("Temp Threshold updated via MQTT to: "); Serial.println(tempThreshold);
      thresholdUpdated = true;
      dataUpdated = true;
      publishDeviceFlag = true;
      updateDisplay = true;
    } else {
      Serial.println("Ngưỡng nhiệt độ không hợp lệ!");
    }
  }
  else if (topicStr == TOPIC_CONTROL_HUMI_THRESHOLD) {
    float newThreshold = messageTemp.toFloat();
    if (newThreshold >= 0 && newThreshold <= 100) { // Giới hạn ngưỡng hợp lý
      humiThreshold = newThreshold;
      Serial.print("Humi Threshold updated via MQTT to: "); Serial.println(humiThreshold);
      thresholdUpdated = true;
      dataUpdated = true;
      publishDeviceFlag = true;
      updateDisplay = true;
    } else {
      Serial.println("Ngưỡng độ ẩm không hợp lệ!");
    }
  }
}

// Tác vụ đọc dữ liệu cảm biến
void TaskReadSensor(void *pvParameters) {
  while (1) {
    sensors_event_t humidity_event, temp_event;
    if (ahtInitialized) {
      if (aht.getEvent(&humidity_event, &temp_event)) {
        temperature = temp_event.temperature;
        humidity = humidity_event.relative_humidity;
        Serial.println("Đọc AHT20 thành công");
      } else {
        Serial.println("❌ Lỗi đọc dữ liệu từ AHT20!");
        temperature = 0;
        humidity = 0;
      }
    } else {
      Serial.println("AHT20 không được khởi tạo!");
      temperature = 0;
      humidity = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Delay nhỏ để tránh xung đột I2C

    if (bmpInitialized) {
      pressure = bmp.readPressure() / 100.0;
      altitude = bmp.readAltitude(1013.25);
      if (isnan(pressure) || isnan(altitude)) {
        Serial.println("❌ Lỗi đọc dữ liệu từ BMP280!");
        pressure = 0;
        altitude = 0;
      } else {
        Serial.println("Đọc BMP280 thành công");
      }
    } else {
      Serial.println("BMP280 không được khởi tạo!");
      pressure = 0;
      altitude = 0;
    }

    if (temperature != 0 && humidity != 0) { // Chỉ tăng count nếu dữ liệu hợp lệ
      tempSum += temperature;
      humiSum += humidity;
      if (bmpInitialized && pressure != 0) {
        pressSum += pressure;
      }
      count++;
    } else {
      Serial.println("Dữ liệu không hợp lệ, bỏ qua tính trung bình");
    }
    countTime++;

    Serial.print("Nhiệt độ: "); Serial.print(temperature); Serial.println("°C");
    Serial.print("Độ ẩm: "); Serial.print(humidity); Serial.println("%");
    Serial.print("Áp suất: "); Serial.print(pressure); Serial.println(" hPa");
    Serial.print("Độ cao: "); Serial.print(altitude); Serial.println(" m");
    Serial.println("-------------------------");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// Tác vụ tính trung bình dữ liệu cảm biến
void TaskPrintData(void *pvParameters) {
  while (1) {
    if (countTime >= 15) { // Trung bình mỗi 30 giây (15 lần đọc, mỗi lần 2 giây)
      if (count > 0) {
        avgTemperature = tempSum / count;
        avgHumidity = humiSum / count;
        avgPressure = bmpInitialized ? (pressSum / count) : 0;
        avgAltitude = bmpInitialized ? bmp.readAltitude(1013.25) : 0;

        Serial.println("--------------");
        Serial.println("Trung bình 30s:");
        Serial.print("Nhiệt độ trung bình: "); Serial.print(avgTemperature); Serial.println("°C");
        Serial.print("Độ ẩm trung bình: "); Serial.print(avgHumidity); Serial.println("%");
        Serial.print("Áp suất trung bình: "); Serial.print(avgPressure); Serial.println(" hPa");
        Serial.print("Độ cao trung bình: "); Serial.print(avgAltitude); Serial.println(" m");
        Serial.println("--------------");

        updateDisplay = true;
        dataUpdated = true;
        publishSensorFlag = true;
        publishDeviceFlag = true;

        tempSum = 0;
        humiSum = 0;
        pressSum = 0;
        count = 0;
        countTime = 0;
      } else {
        Serial.println("Không có dữ liệu hợp lệ để tính trung bình!");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}

// Tác vụ hiển thị lên OLED
void TaskDisplay(void *pvParameters) {
  while (1) {
    if (!isAPMode && updateDisplay && oledInitialized) {
      Serial.println("Cập nhật hiển thị OLED...");
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);

      display.print("Mode: "); display.println(isAutoMode ? "Auto" : "Manual");
      display.print("Temp: "); display.print(avgTemperature, 1); display.println(" C");
      display.print("Hum: "); display.print(avgHumidity, 1); display.println(" %");
      display.print("Press: "); display.print(avgPressure, 1); display.println(" hPa");
      display.print("Alt: "); display.print(avgAltitude, 1); display.println(" m");
      display.print("Dev1: "); display.println(device1State ? "ON" : "OFF");
      display.print("Dev2: "); display.println(device2State ? "ON" : "OFF");

      display.display();
      Serial.println("Đã hiển thị lên OLED.");
      updateDisplay = false;
    } else if (!updateDisplay) {
      Serial.println("Không có cập nhật hiển thị (updateDisplay = false).");
    } else if (!oledInitialized) {
      Serial.println("OLED không được khởi tạo, không thể hiển thị!");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Tác vụ điều khiển tự động và xử lý nút bấm
void TaskControl(void *pvParameters) {
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_DEVICE1, INPUT_PULLUP);
  pinMode(BUTTON_DEVICE2, INPUT_PULLUP);
  pinMode(DEVICE1_PIN, OUTPUT);
  pinMode(DEVICE2_PIN, OUTPUT);

  digitalWrite(DEVICE1_PIN, !device1State); // Relay đảo logic
  digitalWrite(DEVICE2_PIN, !device2State);

  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 200;

  while (1) {
    unsigned long currentTime = millis();

    // Xử lý nút bấm chế độ
    if (digitalRead(BUTTON_MODE) == LOW && (currentTime - lastDebounceTime) > debounceDelay) {
      isAutoMode = !isAutoMode;
      Serial.print("Mode switched to: "); Serial.println(isAutoMode ? "Auto" : "Manual");
      
      if (isAutoMode) {
        dataUpdated = true;
      } else {
        device1State = false;
        device2State = false;
        digitalWrite(DEVICE1_PIN, !device1State);
        digitalWrite(DEVICE2_PIN, !device2State);
        publishDeviceFlag = true;
      }
      updateDisplay = true;
      lastDebounceTime = currentTime;
    }

    // Xử lý nút bấm điều khiển thiết bị (chỉ ở chế độ thủ công)
    if (!isAutoMode) {
      if (digitalRead(BUTTON_DEVICE1) == LOW && (currentTime - lastDebounceTime) > debounceDelay) {
        device1State = !device1State;
        digitalWrite(DEVICE1_PIN, !device1State);
        Serial.print("Device 1 switched to: "); Serial.println(device1State ? "ON" : "OFF");
        updateDisplay = true;
        publishDeviceFlag = true;
        lastDebounceTime = currentTime;
      }

      if (digitalRead(BUTTON_DEVICE2) == LOW && (currentTime - lastDebounceTime) > debounceDelay) {
        device2State = !device2State;
        digitalWrite(DEVICE2_PIN, !device2State);
        Serial.print("Device 2 switched to: "); Serial.println(device2State ? "ON" : "OFF");
        updateDisplay = true;
        publishDeviceFlag = true;
        lastDebounceTime = currentTime;
      }
    }

    // Logic điều khiển tự động
    if (isAutoMode && dataUpdated) {
      bool prevDevice1 = device1State;
      bool prevDevice2 = device2State;

      device1State = (avgTemperature < tempThreshold); // Heater bật nếu nhiệt độ thấp
      device2State = (avgHumidity < humiThreshold);   // Humidifier bật nếu độ ẩm thấp

      if (prevDevice1 != device1State || prevDevice2 != device2State) {
        digitalWrite(DEVICE1_PIN, !device1State);
        digitalWrite(DEVICE2_PIN, !device2State);

        Serial.print("Auto Mode - Temp: "); Serial.print(avgTemperature); Serial.println("C");
        Serial.print("Humidity: "); Serial.print(avgHumidity); Serial.println("%");
        Serial.print("Device 1: "); Serial.print(device1State ? "ON" : "OFF");
        Serial.print(", Device 2: "); Serial.println(device2State ? "ON" : "OFF");
        publishDeviceFlag = true;
        updateDisplay = true;
      }
      dataUpdated = false;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// Tác vụ xử lý MQTT
void TaskMQTT(void *pvParameters) {
  unsigned long lastReconnectAttempt = 0;
  const unsigned long reconnectInterval = 5000; // Thử kết nối lại mỗi 5 giây

  while (1) {
    unsigned long currentMillis = millis();

    if (!isAPMode) {
      if (WiFi.status() != WL_CONNECTED || !client.connected()) {
        Serial.println("Connection lost, attempting to reconnect...");

        if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
          if (!reconnect()) {
            Serial.println("Reconnection failed");
            Serial.print("Current failed attempts: ");
            Serial.println(failedReconnectAttempts);

            if (failedReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
              Serial.println("Too many failed attempts, switching to AP mode");
              startAPMode();
            }
          } else {
            Serial.println("Reconnection successful");
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
    }

    if (currentMillis - lastReconnectAttempt >= RESET_TIMEOUT) {
      Serial.println("Connection timeout, resetting ESP32...");
      ESP.restart();
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Tác vụ nhận dữ liệu MQTT
void TaskMQTTSubscribe(void *pvParameters) {
  client.setCallback(callback);

  while (1) {
    if (!client.connected()) {
      if (!reconnect()) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Đợi Serial khởi tạo

  Wire.begin();
  scanI2C();
  oledInitialized = initializeOLED();
  if (!oledInitialized) {
    Serial.println("Không thể khởi tạo OLED. Vui lòng kiểm tra phần cứng!");
  }

  int sensorInitAttempts = 0;
  const int maxSensorInitAttempts = 3;

  // Thử khởi tạo AHT20 nhiều lần
  while (!aht.begin() && sensorInitAttempts < maxSensorInitAttempts) {
    Serial.println("Không tìm thấy cảm biến AHT20! Thử lại...");
    sensorInitAttempts++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  if (aht.begin()) {
    Serial.println("Đã tìm thấy AHT20!");
    ahtInitialized = true;
  } else {
    Serial.println("❌ Không thể khởi tạo AHT20 sau nhiều lần thử!");
    ahtInitialized = false;
  }

  sensorInitAttempts = 0;
  // Thử khởi tạo BMP280 nhiều lần
  while (!bmp.begin(0x77) && sensorInitAttempts < maxSensorInitAttempts) {
    Serial.println("Không tìm thấy cảm biến BMP280 tại địa chỉ 0x77! Thử lại...");
    sensorInitAttempts++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  if (bmp.begin(0x77)) {
    Serial.println("Đã tìm thấy BMP280 tại địa chỉ 0x77!");
    bmpInitialized = true;
  } else {
    Serial.println("❌ Không thể khởi tạo BMP280 sau nhiều lần thử!");
    bmpInitialized = false;
  }

  if (oledInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Khoi dong...");
    display.display();
    delay(2000);
  }

  loadCredentials();

  if (setup_wifi()) {
    client.setServer(mqtt_server, mqtt_port);
    if (!reconnect()) {
      Serial.println("Initial MQTT connection failed");
    }
  } else {
    Serial.println("Initial WiFi connection failed");
    startAPMode();
  }

  xTaskCreatePinnedToCore(TaskReadSensor, "TaskRead", 4096, NULL, 3, &TaskReadHandle, 1);
  xTaskCreatePinnedToCore(TaskPrintData, "TaskPrint", 4096, NULL, 1, &TaskPrintHandle, 1);
  xTaskCreatePinnedToCore(TaskDisplay, "TaskDisplay", 4096, NULL, 1, &TaskDisplayHandle, 0);
  xTaskCreatePinnedToCore(TaskControl, "TaskControl", 4096, NULL, 1, &TaskControlHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTT, "TaskMQTT", 4096, NULL, 1, &TaskMQTTHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTTSubscribe, "TaskMQTTSubscribe", 4096, NULL, 2, &TaskMQTTSubscribeHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
