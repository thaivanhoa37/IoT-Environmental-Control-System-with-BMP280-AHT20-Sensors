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

// Cấu hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Khởi tạo cảm biến
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
bool bmpInitialized = false;
bool ahtInitialized = false;

// Định nghĩa chân
#define DEVICE1_PIN 26 // Relay 1 (heater)
#define DEVICE2_PIN 27 // Relay 2 (humidifier)

// Cấu hình EEPROM
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MQTT_IP_ADDR 64

// Thông tin WiFi và MQTT
char ssid[32] = "vanhoa";
char password[32] = "11111111";
char mqtt_server[32] = "192.168.137.145";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_IOT_Controller";

float temperature = 0, humidity = 0, pressure = 0, altitude = 0;
float tempThreshold = 25.0;
float humiThreshold = 60.0;
bool isAutoMode = false;
bool device1State = false;
bool device2State = false;
bool isConnected = false;
bool isAPMode = false;

WiFiClient espClient;
PubSubClient client(espClient);

// Khai báo server toàn cục
AsyncWebServer server(80);

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
  Serial.println("Không thể khởi tạo OLED ở địa chỉ đã chọn!");
  return false;
}

void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASS_ADDR, password);
  EEPROM.get(MQTT_IP_ADDR, mqtt_server);
  EEPROM.end();
  Serial.println("Đã tải thông tin:");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("MQTT Server: "); Serial.println(mqtt_server);
}

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
  Serial.println("Đã lưu thông tin mới:");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("MQTT Server: "); Serial.println(mqtt_server);
}

bool setup_wifi() {
  WiFi.begin(ssid, password);
  int attempts = 0;
  const int maxAttempts = 20;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nĐã kết nối WiFi");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("\nKết nối WiFi thất bại");
  return false;
}

bool connect_mqtt() {
  client.setServer(mqtt_server, mqtt_port);
  int retryCount = 0;
  while (!client.connected() && retryCount < 3) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
      retryCount++;
    }
  }
  Serial.println("Kết nối MQTT thất bại");
  return false;
}

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

void startAPMode() {
  WiFi.disconnect(true);
  delay(1000);
  isAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678", 6);
  Serial.println("Đã khởi động chế độ AP");
  Serial.print("IP AP: "); Serial.println(WiFi.softAPIP());

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("AP Mode");
  display.println("SSID: ESP32_Config");
  display.println("Pass: 12345678");
  display.println("IP: 192.168.4.1");
  display.display();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Nhận yêu cầu GET tại /");
    request->send(200, "text/html", getConfigPage());
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Nhận yêu cầu POST tại /save");
    if (request->hasArg("ssid") && request->hasArg("pass") && request->hasArg("mqtt")) {
      String newSsid = request->arg("ssid");
      String newPass = request->arg("pass");
      String newMqtt = request->arg("mqtt");
      Serial.println("Lưu thông tin mới từ web...");
      saveCredentials(newSsid.c_str(), newPass.c_str(), newMqtt.c_str());
      request->send(200, "text/html", "<html><body><h1>Đã lưu! Đang kiểm tra kết nối...</h1></body></html>");
      delay(1000);
      request->client()->close();
      server.end();
      WiFi.softAPdisconnect(true);
      delay(1000);
      isConnected = setup_wifi() && connect_mqtt();
      if (isConnected) {
        Serial.println("Kết nối thành công! Chuyển sang màn hình cảm biến.");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Ket noi thanh cong!");
        display.display();
        delay(2000);
        isAPMode = false;
      } else {
        Serial.println("Kết nối thất bại! Vào lại AP mode.");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Ket noi that bai!");
        display.display();
        delay(2000);
        startAPMode();
      }
    } else {
      request->send(400, "text/html", "<html><body><h1>Dữ liệu không hợp lệ</h1></body></html>");
    }
  });

  server.begin();
  Serial.println("Async Web Server started");
}

void readSensors() {
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
    temperature = 0;
    humidity = 0;
  }

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
    pressure = 0;
    altitude = 0;
  }

  Serial.print("Nhiệt độ: "); Serial.print(temperature); Serial.println("°C");
  Serial.print("Độ ẩm: "); Serial.print(humidity); Serial.println("%");
  Serial.print("Áp suất: "); Serial.print(pressure); Serial.println(" hPa");
  Serial.print("Độ cao: "); Serial.print(altitude); Serial.println(" m");
  Serial.println("-------------------------");
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Mode: "); display.println(isAutoMode ? "Auto" : "Manual");
  display.print("Temp: "); display.print(temperature, 1); display.println(" C");
  display.print("Hum: "); display.print(humidity, 1); display.println(" %");
  display.print("Press: "); display.print(pressure, 1); display.println(" hPa");
  display.print("Alt: "); display.print(altitude, 1); display.println(" m");
  display.print("R1: "); display.println(device1State ? "ON" : "OFF");
  display.print("R2: "); display.println(device2State ? "ON" : "OFF");
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin();
  scanI2C();
  if (!initializeOLED()) {
    Serial.println("Không thể khởi tạo OLED. Vui lòng kiểm tra phần cứng!");
  }

  if (!aht.begin()) {
    Serial.println("Không tìm thấy cảm biến AHT20!");
    ahtInitialized = false;
  } else {
    Serial.println("Đã tìm thấy AHT20!");
    ahtInitialized = true;
  }

  if (!bmp.begin(0x77)) {
    Serial.println("Không tìm thấy cảm biến BMP280 tại địa chỉ 0x77!");
    bmpInitialized = false;
  } else {
    Serial.println("Đã tìm thấy BMP280 tại địa chỉ 0x77!");
    bmpInitialized = true;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Khoi dong...");
  display.display();
  delay(2000);

  loadCredentials();
  Serial.println("Vào chế độ AP ngay từ đầu...");
  startAPMode();

  pinMode(DEVICE1_PIN, OUTPUT);
  pinMode(DEVICE2_PIN, OUTPUT);
  digitalWrite(DEVICE1_PIN, !device1State);
  digitalWrite(DEVICE2_PIN, !device2State);
}

void loop() {
  if (isConnected && !isAPMode) {
    readSensors();
    if (isAutoMode) {
      device1State = (temperature < tempThreshold);
      device2State = (humidity < humiThreshold);
      digitalWrite(DEVICE1_PIN, !device1State);
      digitalWrite(DEVICE2_PIN, !device2State);
    }
    updateDisplay();
    delay(2000);
  }
  Serial.println("ESP32 đang chạy...");
  delay(1000);
}