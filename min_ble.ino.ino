#include <WiFi.h>
#include <NimBLEDevice.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "MQ135.h"

#define EEPROM_SIZE 512
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define RESET_BUTTON_PIN 27
#define WARN_LED_DO 33    // hoat dong
#define WARN_LED_VANG 25  // khong ket noi
#define WARN_LED_XANH 26  // chua cai dat

/*
DTH: 4
LED: 2
CO2: 35
BUTTON: 34
LIGHSENSOR: 32
HUMITYSOIL: 33
RAINSENSOR: 25
*/

#define DHTPIN 23
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define LED_CONTROLL 2
#define PIN_MQ135 22
MQ135 mq135_sensor = MQ135(PIN_MQ135);
#define LIGH_SENSOR 34
#define SOIL_SENSOR 33
#define RAIN_SENSOR 35

/*
Status device:
Không load được dữ liệu từ EEPROM: "no_data"
Không kết nối được WiFi: "wifi_error"
Không kết nối được MQTT: "mqtt_error"
Kết nối WiFi và MQTT thành công: "connected"
*/
String statusDevice = "";

// Khai báo các biến toàn cục
bool resetButtonPressed = false;         // Biến lưu trạng thái nút reset
unsigned long resetButtonPressTime = 0;  // Biến lưu thời gian nhấn nút reset

WiFiClient espClient;
PubSubClient client(espClient);

char wifiSSID[32];
char wifiPass[64];
char mqttUser[32];
char mqttServer[32];
char mqttPass[64];
char *mqttTopic = "v1/devices/me/telemetry";
int mqttPort = 1883;

bool wifiConnected = false;
bool mqttConnected = false;
bool isChangedData = false;

NimBLEServer *pServer;
NimBLECharacteristic *pCharacteristic;
String rxValue = "";

void setWarnLed(const int type);
void setupBLE();
bool connectToWiFi(const char *ssid, const char *password);
bool connectToMQTT();
void sendDataMqtt();
void sendDHT11Data();
void onBLEReceive(String jsonData);
void saveCredentialsToEEPROM();
void loadCredentialsFromEEPROM();
void handleResetButton();
void resetDevice();
void callback(char *topic, byte *payload, unsigned int length);

class MyBLECallbacks : public BLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    String rxData = pCharacteristic->getValue().c_str();
    if (rxData.length() > 0) {  // Kiểm tra dữ liệu có độ dài lớn hơn 0
      Serial.println("Received credentials over BLE");
      rxValue += rxData;  // Thêm dữ liệu nhận vào chuỗi rxValue
      Serial.println("Received Data: " + rxValue);
      if (rxValue.indexOf(';') != -1) {
        onBLEReceive(rxValue);
        Serial.println("BLECALLBACK.LOG -> " + rxValue);
        rxValue = "";
      }
    } else {
      Serial.println("Received empty data");
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_CONTROLL, OUTPUT);
  pinMode(WARN_LED_DO, OUTPUT);
  pinMode(WARN_LED_VANG, OUTPUT);
  pinMode(WARN_LED_XANH, OUTPUT);
  client.setCallback(callback);
  dht.begin();
  EEPROM.begin(EEPROM_SIZE);
  loadCredentialsFromEEPROM();
  setWarnLed(0);
  if (statusDevice == "no_data") {
    setWarnLed(2);
    Serial.println("No data found in EEPROM");
    setupBLE();
    while (statusDevice != "connected") {
      if (isChangedData) {
        connectToWiFi(wifiSSID, wifiPass);
        if (wifiConnected) {
          connectToMQTT();
        }
        if (mqttConnected) {
          String message = "SUCCESS";
          pCharacteristic->setValue(message.c_str());
          pCharacteristic->notify();
          Serial.println("BLE notification sent: " + message);
          saveCredentialsToEEPROM();
          statusDevice = "connected";
          isChangedData = false;
        }
      }
      delay(2000);
    }
    BLEDevice::deinit();          // Tắt BLE stack
    esp_bt_controller_disable();  // Tắt Bluetooth controller
    setWarnLed(1);
  }
  connectToWiFi(wifiSSID, wifiPass);
}

int timeSend = 60 * 1000;
unsigned long lastTime = 0;

void loop() {
  unsigned long currentTime = millis();  // Luôn dùng unsigned long

  handleResetButton();

  // Xử lý kết nối WiFi
  if (WiFi.status() != WL_CONNECTED) {
    setWarnLed(3);
    WiFi.disconnect();
    WiFi.reconnect();
    delay(1000);
    return;
  }

  // Xử lý kết nối MQTT
  if (!mqttConnected || !client.connected()) {
    setWarnLed(3);
    connectToMQTT();
    if (!mqttConnected) {
      delay(1000);
      return;
    }
  }

  setWarnLed(1);

  // Gửi dữ liệu định kỳ (tự động xử lý tràn số)
  if (currentTime - lastTime >= timeSend) {
    sendDataMqtt();
    lastTime = currentTime;
  }

  client.loop();
  delay(100);  // Giảm delay để phản hồi nhanh hơn
}

void sendDataMqtt() {
  // // air
  // float humidity = dht.readHumidity();
  // float temperature = dht.readTemperature();

  // // ppm
  // float correctedPPM = mq135_sensor.getCorrectedPPM(temperature, humidity);

  // light
  int lightPercentage = map(analogRead(LIGH_SENSOR), 0, 1023, 0, 100);

  // // soil
  // int moisturePercent = map(analogRead(SOIL_SENSOR), 1023, 0, 0, 100);

  // rain
  int rainPercentage = map(analogRead(RAIN_SENSOR), 0, 1023, 0, 100);

  // total
  String payload = "{";
  // payload += "\"humidity\":" + String(humidity, 1) + ",";
  // payload += "\"temperature\":" + String(temperature, 1) + ",";
  // payload += "\"air_quality\":" + String(correctedPPM, 1);
  payload += "\"light\":" + String(lightPercentage) + ",";
  // payload += "\"soil_moisture\":" + String(moisturePercent) + ",";
  payload += "\"rain\":" + String(rainPercentage);
  payload += "}";
  client.publish(mqttTopic, payload.c_str());
  Serial.println("Data sent to MQTT: " + payload);
}

void setWarnLed(const int type) {
  bool red = false, green = false, blue = false;
  if (type == 1) {
    red = true;
  }
  if (type == 2) {
    green = true;
  }
  if (type == 3) {
    blue = true;
  }
  digitalWrite(WARN_LED_DO, red);
  digitalWrite(WARN_LED_VANG, green);
  digitalWrite(WARN_LED_XANH, blue);
}

bool connectToWiFi(const char *ssid, const char *password) {
  Serial.println("Connecting to WiFi...");
  Serial.println("CONNECT WITH " + String(ssid) + " | " + String(password));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startTime > 30000) {
      Serial.println("\nFailed to connect to WiFi");
      wifiConnected = false;
      statusDevice = "no_connected";
      return false;
    }
  }
  Serial.println("\nWiFi connected");
  statusDevice = "wifi_connected";
  wifiConnected = true;
  return true;
}

bool connectToMQTT() {
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  Serial.print("Connecting to MQTT...");
  if (client.connect("ESP32Client", mqttUser, mqttPass)) {
    Serial.println("MQTT connected");
    mqttConnected = true;
    // Đăng ký nhận thông tin Shared Attributes khi có thay đổi
    client.subscribe("v1/devices/me/attributes");
    // Gửi yêu cầu lấy giá trị Shared Attributes
    String request = "{\"sharedKeys\":\"lightState\"}";
    client.publish("v1/devices/me/attributes/request/1", request.c_str());
    if (statusDevice == "wifi_connected") {
      statusDevice = "connected";
      setWarnLed(1);
    }
    mqttConnected = true;
    return true;
  } else {
    Serial.print("Failed to connect to MQTT, rc=");
    Serial.println(client.state());
    mqttConnected = false;
    return false;
  }
}

void sendDHT11Data() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  if (isnan(humidity)) {
    Serial.println("Failed to read from DHT11 sensor!");
    return;
  }
  String payload = "{\"humidity\": " + String(humidity) + ", \"temperature\":" + String(temperature) + "}";
  client.publish(mqttTopic, payload.c_str());
  Serial.println("Data sent to MQTT: " + payload);
}

void setupBLE() {
  NimBLEDevice::init("ESP32_NimBLE_LED");
  NimBLEServer *pServer = NimBLEDevice::createServer();

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pCharacteristic->setCallbacks(new MyBLECallbacks());

  pService->start();

  pServer->getAdvertising()->start();  // Start advertising BLE service
  Serial.println("BLE setup complete, waiting for credentials...");
}

void onBLEReceive(String jsonData) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }
  if (doc.containsKey("reset") && doc["reset"] == true) {
    Serial.println("Reset command received via BLE. Resetting device...");
    resetDevice();
    return;
  }
  strcpy(wifiSSID, doc["ssid"]);
  strcpy(wifiPass, doc["password"]);
  strcpy(mqttUser, doc["mqttUser"]);
  strcpy(mqttServer, doc["mqttServer"]);
  mqttPort = doc["mqttPort"];
  Serial.printf("onBLEReceive.LOG -> SSI: %s\n", wifiSSID);
  Serial.printf("onBLEReceive.LOG -> PASSWORD: %s\n", wifiPass);
  Serial.printf("onBLEReceive.LOG -> MQTTUSER: %s\n", mqttUser);
  Serial.printf("onBLEReceive.LOG -> MQTTSERVER: %s\n", mqttServer);
  Serial.printf("onBLERecevie.LOG -> MQTTPORT: %d\n", mqttPort);
  isChangedData = true;
}

void saveCredentialsToEEPROM() {
  EEPROM.writeString(0, wifiSSID);      // 0-31
  EEPROM.writeString(32, wifiPass);     // 32-95
  EEPROM.writeString(96, mqttUser);     // 96-127
  EEPROM.writeString(128, mqttPass);    // 128-159
  EEPROM.writeString(160, mqttServer);  // 160-191
  // mqttPort
  EEPROM.write(192, (uint8_t)(mqttPort >> 8));    //Byte cao
  EEPROM.write(193, (uint8_t)(mqttPort & 0xFF));  // Byte thấp

  EEPROM.commit();
  Serial.printf("SAVE_EROM.LOG -> SSI: %s\n", wifiSSID);
  Serial.printf("SAVE_EROM.LOG -> PASSWORD: %s\n", wifiPass);
  Serial.printf("SAVE_EROM.LOG -> MQTTUSER: %s\n", mqttUser);
  Serial.printf("SAVE_EROM.LOG -> MQTTSERVER: %s\n", mqttServer);
  Serial.printf("SAVE_EROM.LOAD.LOG -> MQTTPORT: %d\n", mqttPort);
  Serial.println("Credentials saved to EEPROM.");
}

void loadCredentialsFromEEPROM() {
  EEPROM.readString(0).toCharArray(wifiSSID, sizeof(wifiSSID));
  EEPROM.readString(32).toCharArray(wifiPass, sizeof(wifiPass));
  EEPROM.readString(96).toCharArray(mqttUser, sizeof(mqttUser));
  EEPROM.readString(128).toCharArray(mqttPass, sizeof(mqttPass));
  EEPROM.readString(160).toCharArray(mqttServer, sizeof(mqttServer));
  // read mqttPort
  mqttPort = (EEPROM.read(192) << 8) | EEPROM.read(193);

  Serial.printf("SAVE_EROM.LOAD.LOG -> SSI: %s\n", wifiSSID);
  Serial.printf("SAVE_EROM.LOAD.LOG -> PASSWORD: %s\n", wifiPass);
  Serial.printf("SAVE_EROM.LOAD.LOG -> MQTTUSER: %s\n", mqttUser);
  Serial.printf("SAVE_EROM.LOAD.LOG -> MQTTSERVER: %s\n", mqttServer);
  Serial.printf("SAVE_EROM.LOAD.LOG -> MQTTPORT: %d\n", mqttPort);

  if (strlen(wifiSSID) == 0 || strlen(wifiPass) == 0 || strlen(mqttUser) == 0 || strlen(mqttServer) == 0) {
    statusDevice = "no_data";
    Serial.println("No data found in EEPROM");
  } else {
    statusDevice = "data_found";
    Serial.println("Credentials loaded from EEPROM.");
  }
}

#define DEBOUNCE_DELAY 50  // 50ms để lọc nhiễu

void handleResetButton() {
  static unsigned long lastDebounceTime = 0;
  int buttonState = digitalRead(RESET_BUTTON_PIN);

  if (buttonState == HIGH) {
    if (!resetButtonPressed && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
      resetButtonPressTime = millis();
      resetButtonPressed = true;
      lastDebounceTime = millis();
    } else if (resetButtonPressed && millis() - resetButtonPressTime > 3000) {
      Serial.println("Reset button held for 10 seconds. Resetting device...");
      resetDevice();
      resetButtonPressed = false;
    }
  } else {
    resetButtonPressed = false;
  }
}

void resetDevice() {
  memset(wifiSSID, 0, sizeof(wifiSSID));
  memset(wifiPass, 0, sizeof(wifiPass));
  memset(mqttUser, 0, sizeof(mqttUser));
  memset(mqttServer, 0, sizeof(mqttServer));
  mqttPort = 0;
  saveCredentialsToEEPROM();
  ESP.restart();
}

void callback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received: ");
  Serial.println(message);
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);
  if (!error) {
    bool lightState = doc["lightState"];
    if (doc["shared"]["lightState"])
      lightState = doc["shared"]["lightState"];
    if (doc["shared"]["timeSend"])
      timeSend = doc["shared"]["timeSend"];
    if (doc["timeSend"])
      timeSend = doc["timeSend"];
    if (lightState) {
      digitalWrite(LED_CONTROLL, HIGH);
    } else {
      digitalWrite(LED_CONTROLL, LOW);
    }
  }
}