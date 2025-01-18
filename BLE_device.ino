#include <WiFi.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// DHT sensor configuration
#define DHTPIN 4 // GPIO pin where the DHT11 is connected
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor

// EEPROM size for storing credentials
#define EEPROM_SIZE 512

// BLE service UUIDs and characteristics
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// MQTT configuration
WiFiClient espClient;
PubSubClient client(espClient);

// Reset button configuration
#define RESET_BUTTON_PIN 5 // GPIO pin for reset button
unsigned long resetButtonPressTime = 0;
bool resetButtonPressed = false;

// Wi-Fi and MQTT credentials stored in EEPROM
char wifiSSID[32];
char wifiPass[64];
char *mqttServer = "demo.thingsboard.io";
char mqttUser[32];
char *mqttPass = "";
char *mqttTopic = "v1/devices/me/telemetry";

// Connection status flags
bool wifiConnected = false;
bool mqttConnected = false;

// BLE characteristic
BLECharacteristic *pCharacteristic;
BLEServer *pServer;  // Store BLEServer instance globally
String rxValue = ""; // Use String instead of std::string to match getValue()

// Function prototypes
void setupBLE();
void connectToWiFi(const char *ssid, const char *password);
void connectToMQTT();
void sendDHT11Data();
void onBLEReceive(String jsonData); // Change to String
void saveCredentialsToEEPROM();
void loadCredentialsFromEEPROM();


// Function to handle reset button
void handleResetButton() {
  if (digitalRead(RESET_BUTTON_PIN) == LOW) { // Button is pressed (assuming active LOW)
    if (!resetButtonPressed) {//neu ma nut reset button da nhan
      resetButtonPressTime = millis(); // ghi lai tgian khi nut dc nhan
      resetButtonPressed = true;
    } else if (millis() - resetButtonPressTime > 10000) { // Button held for 10 seconds
      Serial.println("Reset button held for 10 seconds. Resetting device...");
      resetDevice();
    }
  } else {
    resetButtonPressed = false;
  }
}
// Modify resetDevice() to send BLE notification
void resetDevice() {
  // Clear Wi-Fi and MQTT credentials from EEPROM
  memset(wifiSSID, 0, sizeof(wifiSSID));
  memset(wifiPass, 0, sizeof(wifiPass));
  memset(mqttUser, 0, sizeof(mqttUser));
  saveCredentialsToEEPROM();

  // Send BLE notification
  String message = "RESET_SUCCESS";
  pCharacteristic->setValue(message.c_str());
  pCharacteristic->notify();
  Serial.println("BLE notification sent: " + message);
  //gui thong bao BLE
  sendBLEData("RESET", "đã xoá thông tin xác thực của thiết bị");
  Serial.println("da hoan tat thiet lap lai thiet bi, dang khoi dong lai...");
  // Restart the device
  ESP.restart();
}
void sendBLEData(String status, String message) {
  DynamicJsonDocument doc(256);
  doc["status"] = status;
  doc["message"] = message;

  String jsonString;
  serializeJson(doc, jsonString);  // Serialize JSON to string

  pCharacteristic->setValue(jsonString.c_str());  // Set BLE notification value
  pCharacteristic->notify();                      // Send BLE notification
  Serial.println("BLE notification sent: " + jsonString);
}


// BLE callback class to handle received credentials
class MyBLECallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    String rxData = pCharacteristic->getValue().c_str(); // Lấy dữ liệu nhận qua BLE và chuyển thành chuỗi String
    if (rxData.length() > 0)                             // Kiểm tra dữ liệu có độ dài lớn hơn 0
    {
      Serial.println("Received credentials over BLE");
      rxValue += rxData;                           // Thêm dữ liệu nhận vào chuỗi rxValue
      Serial.println("Received Data: " + rxValue); // In dữ liệu nhận được

      // Kiểm tra nếu chuỗi chứa dấu phân cách ';' (nếu bạn đang sử dụng để xác định khi nào chuỗi kết thúc)
      if (rxValue.indexOf(';') != -1) // Sử dụng dấu phân cách (nếu có) để xác định kết thúc tin nhắn
      {
        onBLEReceive(rxValue); // Gọi hàm xử lý dữ liệu JSON
        Serial.println("BLECALLBACK.LOG -> " + rxValue);
        rxValue = ""; // Xóa rxValue sau khi xử lý xong 
      }
    }
    else
    {
      Serial.println("Received empty data");
    }
  }
};

// Setup function: initializes sensor, EEPROM, and attempts connections
void setup() {
  Serial.begin(115200);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP); // Initialize reset button pin
  dht.begin();
  EEPROM.begin(EEPROM_SIZE);
  loadCredentialsFromEEPROM();
  connectToWiFi(wifiSSID, wifiPass);
  setupBLE();
  connectToMQTT();
}

bool countConnectMqtt = 0;

// Main loop: handles connection and sensor data transmission
void loop() {
  handleResetButton(); // Check reset button state
  if (WiFi.status() == WL_CONNECTED && mqttConnected) {
    sendDHT11Data();
    delay(10000);
  } else {
    Serial.print("[-]");
    delay(1000);
  }
}
// Connect to Wi-Fi using provided SSID and password
void connectToWiFi(const char *ssid, const char *password) {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);          // Start Wi-Fi connection
  unsigned long startTime = millis();  //ghi lai thoi gian bat dau qtrinh ketnoi wifi
  const unsigned long tgian_cho = 30000;  // khoang tgian chờ tối đa
  // Attempt connection for up to 10 seconds
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);  //dung lai 500mns giua cac lan ktra trang thai ket noi
    Serial.print(".");
    if (millis() - startTime > tgian_cho)  //ham milis(tra ve tgian htai)
    {                                  // Timeout after 10 seconds
               //ktra xem ke tu khi qua trinh ket noi bat dau chay,tgian htai(milis) - tgian bat dau ket noi > 30s in ra ket noi ko thanh cong
      Serial.println("\nFailed to connect to WiFi");
      wifiConnected = false;
      return;
    }
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  wifiConnected = true;
}

// Connect to MQTT broker using stored credentials
void connectToMQTT()
{
  client.setServer(mqttServer, 1883); // Set MQTT server
  Serial.print("Connecting to MQTT...");

  // Attempt MQTT connection
  if (client.connect("ESP32Client", mqttUser, mqttPass))
  {
    Serial.println("MQTT connected");
    mqttConnected = true;
  }
  else
  {
    Serial.print("Failed to connect to MQTT, rc=");
    Serial.println(client.state());
    mqttConnected = false;
  }
}

// Send DHT11 sensor data (humidity) to MQTT
void sendDHT11Data()
{
  float humidity = dht.readHumidity(); // Read humidity from DHT11 sensor
  float temperature = dht.readTemperature();
  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println("Failed to read from DHT11 sensor!");
    return;
  }

  // Create JSON payload with humidity data
  String payload = "{\"humidity\": " + String(humidity) + ", \"temperature\":" + String(temperature) + "}";
  client.publish(mqttTopic, payload.c_str()); // Publish data to MQTT
  Serial.println("Data sent to MQTT: " + payload);
}

// Initialize BLE service for receiving credentials
void setupBLE()
{
  BLEDevice::init("ESP32_BLE");                                // Initialize BLE with device name
  pServer = BLEDevice::createServer();                         // Create BLE server and store in global variable
  BLEService *pService = pServer->createService(SERVICE_UUID); // Create BLE service

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setCallbacks(new MyBLECallbacks()); // Set BLE callbacks
  pService->start();                                   // Start BLE service
  pServer->getAdvertising()->start();                  // Start advertising BLE service
  Serial.println("BLE setup complete, waiting for credentials...");
}

// Parse received JSON data and try to connect to WiFi and MQTT
// Modify onBLEReceive() to handle reset command
void onBLEReceive(String jsonData) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    sendBLEData("ERROR", "Invalid JSON format");
    return;
  }

  if (doc.containsKey("action") && doc["action"] == "delete") {
    resetDevice();
    return;
  }

  if (doc.containsKey("ssid") && doc.containsKey("password")) {
    strcpy(wifiSSID, doc["ssid"]);
    strcpy(wifiPass, doc["password"]);
    connectToWiFi(wifiSSID, wifiPass);

    if (wifiConnected) connectToMQTT();
    if (mqttConnected) {
      sendBLEData("SUCCESS", "Connected to WiFi and MQTT");
    } else {
      sendBLEData("ERROR", "Failed to connect to MQTT");
    }
    saveCredentialsToEEPROM();
  }
}


// Save Wi-Fi and MQTT credentials to EEPROM for persistent storage
void saveCredentialsToEEPROM()
{
  EEPROM.writeString(0, wifiSSID);
  EEPROM.writeString(32, wifiPass);
  EEPROM.writeString(96, mqttUser);
  EEPROM.commit(); // Save data to EEPROM
  Serial.printf("SAVE_EROM.LOG -> SSI: %s\n", wifiSSID);
  Serial.printf("SAVE_EROM.LOG -> PASSWORD: %s\n", wifiPass);
  Serial.printf("SAVE_EROM.LOG -> MQTTUSER: %s\n", mqttUser);
  Serial.println("Credentials saved to EEPROM.");
}

// Load Wi-Fi and MQTT credentials from EEPROM
void loadCredentialsFromEEPROM()
{
  EEPROM.readString(0, wifiSSID, sizeof(wifiSSID));
  EEPROM.readString(32, wifiPass, sizeof(mqttPass));
  EEPROM.readString(96, mqttUser, sizeof(mqttUser));
}
