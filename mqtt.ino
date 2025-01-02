#include <WiFi.h>
#include <PubSubClient.h>  
#include <ArduinoJson.h>

// WiFi credentials
const char* wifissid = "Hotboi UTC";
const char* wifipass = "cuongdung123";

// MQTT broker và ThingsBoard credentials
#define broker "demo.thingsboard.io"
// #define clientId "6283f10d-19a5-4075-ba10-5dd4063a02c3"  
const char* clientid = "ESP32Client";
char* mqtttopic = "v1/devices/me/telemetry";
// ThingsBoard
#define TOKEN "M8G4RcEkDmwYtxQf3gwW"  
#define THINGSBOARD_SERVER "demo.thingsboard.io"

bool wificonnected = false;
bool mqttconnected = false;

// GPIO
#define led1 16

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Định nghĩa chính xác hàm callback
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void sendTelemetryData();
void connecttoWiFi(const char* ssid, const char* password);

void setup() {
  Serial.begin(115200);
  pinMode(led1, OUTPUT);
  connecttoWiFi(wifissid, wifipass);
  client.setServer(THINGSBOARD_SERVER, 1883);
  client.setCallback(callback);  // Đăng ký callback và xử lý nhận tin nhắn
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connecttoWiFi(wifissid, wifipass);
  }

  if (!client.connected()) {
    Serial.println("MQTT disconnected, reconnecting...");
    reconnect();
  }

  client.loop();

  // Gửi dữ liệu lên telemetry theo định kỳ
  static unsigned long lastTelemetry = 0;
  if (WiFi.status() == WL_CONNECTED && mqttconnected) {
    if (millis() - lastTelemetry > 5000) {  // Mỗi 5s sẽ tự động gửi
      lastTelemetry = millis();
      sendTelemetryData();
    }
  } else {
    Serial.println("[-] Waiting for connection...");
    delay(1000);
  }
}


// Kết nối Wi-Fi
void connecttoWiFi(const char* wifissid, const char* wifipass) {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(wifissid, wifipass);
  static unsigned long starttime = millis();
  const unsigned long tgian_cho = 30000;  // 30s
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - starttime > tgian_cho) {
      Serial.println("\nFailed to connect to Wi-Fi.");
      wificonnected = false;
      return;
    }
  }
  Serial.println("\nWi-Fi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  wificonnected = true;
}

// Xử lý dữ liệu khi nhận được tin nhắn
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\nMessage received:");
  Serial.print("Topic: ");
  Serial.println(topic);

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message: ");
  Serial.println(message);

  if (message == "ON") {
    digitalWrite(led1, HIGH);
    Serial.println("LED turned ON");
  } else if (message == "OFF") {
    digitalWrite(led1, LOW);
    Serial.println("LED turned OFF");
  } else {
    Serial.println("Unknown message received.");
  }
}

void reconnect() {
  // String clientID = "ESP32Client-" + String(random(0xffff), HEX);
  client.setServer(THINGSBOARD_SERVER, 1883);
  Serial.print("Connecting to MQTT...");
  
  // Thử kết nối cho đến khi thành công
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Kiểm tra kết nối MQTT
    if (client.connect(clientId, TOKEN, NULL)) {
      Serial.println("Connected to MQTT broker!");
      mqttconnected = true;
      break;
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" - Retrying in 5 seconds...");
      delay(5000);  // Thời gian chờ trước khi thử lại
    }
  }
}
void sendTelemetryData() {
  // Dữ liệu telemetry (trạng thái LED)
  String payload = "{\"ledStatus\": \"" + String(digitalRead(led1) ? "ON" : "OFF") + "\"}";

  if (client.publish(mqtttopic, payload.c_str())) {
    Serial.println("Telemetry sent: " + payload);
  } else {
    Serial.println("Failed to send telemetry");
  }
}
