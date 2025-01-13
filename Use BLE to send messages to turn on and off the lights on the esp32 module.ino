// #include<WiFi.h>
#include<BLEDevice.h>
#include<BLEUtils.h>
#include<BLEServer.h>

// ledPin_ GPIO
#define led1 19
//BLE service and characteristics
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// // Connection status flags
// bool wifiConnected = false;
// //wifi
// char wifiSSID[32];
// char wifiPass[64];

// BLE characteristic
BLECharacteristic *pCharacteristic;
BLEServer *pServer;  // Store BLEServer instance globally
String rxValue = ""; // Use String instead of std::string to match getValue()
// Function prototypes
void setupBLE();
// void connectToWiFi(const char *ssid, const char *password);
// void onBLEReceive(String jsonData); // Change to String

//  BLE callback class to handle received credentials
class MyBLECallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    String rxData = pCharacteristic->getValue().c_str(); // Lấy dữ liệu nhận qua BLE và chuyển thành chuỗi String
    if (rxData.length() > 0){                             // Kiểm tra dữ liệu có độ dài lớn hơn 0
      Serial.println("Received credentials over BLE");
      rxValue += rxData;                           // Thêm dữ liệu nhận vào chuỗi rxValue
      Serial.println("Received Data: " + rxValue); // In dữ liệu nhận được
    // on off led
    if(rxValue.indexOf("ON") != -1){
      digitalWrite(led1 , HIGH);
      Serial.println("LED is HIGH");
    }
    else if(rxValue.indexOf("OFF") != -1){
      digitalWrite(led1 , LOW);
      Serial.println("LED is LOW");
    }
    rxValue = ""; // reset rxValue
  }
    else{
      Serial.println("Received empty data");
    }


  }
};
// Setup function: initializes sensor, EEPROM, and attempts connections
void setup(){
  Serial.begin(921600);
  //ledPin GPIO
  pinMode(led1 , OUTPUT);
  // //Attemp to connect to wifi
  // connectToWiFi(wifiSSID, wifiPass);
  setupBLE();
}
void loop(){
  // if (WiFi.status() == WL_CONNECTED ){
  //   delay(10000); // Send data every 10 seconds
  //   Serial.println("connected to wifi");
  // }
}

// Connect to Wi-Fi using provided SSID and password
// void connectToWiFi(const char *ssid, const char *password)
// {
//   Serial.println("Connecting to WiFi...");
//   WiFi.begin(ssid, password); // Start Wi-Fi connection
//   unsigned long startTime = millis();
//   // Attempt connection for up to 10 seconds
//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(500);
//     Serial.print(".");
//     if (millis() - startTime > 30000)
//     { // Timeout after 10 seconds
//       Serial.println("\nFailed to connect to WiFi");
//       wifiConnected = false;
//       return;
//     }
//   }
//   Serial.println("\nWiFi connected");
//   wifiConnected = true;
// }
void setupBLE(){
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
