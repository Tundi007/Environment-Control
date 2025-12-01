/**
 * Example Arduino sketch for posting buffered sensor readings to the
 * Environment Control Spring Boot backend.
 */
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";
const char* DEVICE_ID = "demo-device";
const char* DEVICE_SECRET = "demo-secret";
const char* API_HOST = "your.api.host";
const uint16_t API_PORT = 8443; // HTTPS port
const char* ROOT_CA = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";

struct Reading {
  uint32_t sequence;
  float temperature;
  float humidity;
};

const size_t MAX_RECORDS = 64; // EEPROM-backed ring buffer
uint32_t writeIndex = 0;
uint32_t sendIndex = 0;
String jwtToken;
WiFiClientSecure client;

void saveReading(float temperature, float humidity) {
  Reading reading{writeIndex, temperature, humidity};
  int addr = (writeIndex % MAX_RECORDS) * sizeof(Reading);
  EEPROM.put(addr, reading);
  EEPROM.commit();
  writeIndex++;
}

bool connectWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint8_t retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries++ < 20) {
    delay(500);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool login() {
  if (!client.connect(API_HOST, API_PORT)) return false;
  client.setCACert(ROOT_CA);
  DynamicJsonDocument doc(256);
  doc["deviceId"] = DEVICE_ID;
  doc["secret"] = DEVICE_SECRET;
  String body;
  serializeJson(doc, body);

  client.println(String("POST /api/devices/login HTTP/1.1"));
  client.println(String("Host: ") + API_HOST);
  client.println("Content-Type: application/json");
  client.println(String("Content-Length: ") + body.length());
  client.println();
  client.print(body);

  // Basic response parse
  while (client.connected() && !client.available()) delay(10);
  String response = client.readString();
  int tokenIndex = response.indexOf("\"token\":\"");
  if (tokenIndex < 0) return false;
  int start = tokenIndex + 10;
  int end = response.indexOf('\"', start);
  jwtToken = response.substring(start, end);
  return true;
}

bool pollForUpload() {
  if (jwtToken.isEmpty()) return false;
  if (!client.connect(API_HOST, API_PORT)) return false;
  client.setCACert(ROOT_CA);
  client.println("GET /api/devices/pending-requests?longPoll=true HTTP/1.1");
  client.println(String("Host: ") + API_HOST);
  client.println(String("Authorization: Bearer ") + jwtToken);
  client.println();
  while (client.connected() && !client.available()) delay(10);
  String body = client.readString();
  return body.indexOf("uploadRequested":true) > 0;
}

bool sendBatch() {
  if (jwtToken.isEmpty()) return false;
  if (!client.connect(API_HOST, API_PORT)) return false;
  client.setCACert(ROOT_CA);
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.createNestedArray("records");
  uint32_t cursor = sendIndex;
  for (; cursor < writeIndex && arr.size() < 10; cursor++) {
    Reading r; EEPROM.get((cursor % MAX_RECORDS) * sizeof(Reading), r);
    JsonObject rec = arr.createNestedObject();
    rec["sequenceNumber"] = r.sequence;
    rec["payload"] = String(r.temperature) + "," + String(r.humidity);
  }
  String body; serializeJson(doc, body);
  client.println("POST /api/devices/data HTTP/1.1");
  client.println(String("Host: ") + API_HOST);
  client.println("Content-Type: application/json");
  client.println(String("Authorization: Bearer ") + jwtToken);
  client.println(String("Content-Length: ") + body.length());
  client.println();
  client.print(body);
  while (client.connected() && !client.available()) delay(10);
  String response = client.readString();
  int idx = response.indexOf("lastProcessedSequence");
  if (idx < 0) return false;
  uint32_t acked = response.substring(idx + 23).toInt();
  // Drop acknowledged records
  if (acked >= sendIndex) {
    sendIndex = acked + 1;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(MAX_RECORDS * sizeof(Reading));
  connectWifi();
  login();
}

void loop() {
  // Replace with your own sensor sampling
  saveReading(random(200, 300) / 10.0, random(300, 500) / 10.0);

  if (pollForUpload()) {
    uint8_t attempts = 0;
    while (sendIndex < writeIndex && attempts++ < 5) {
      if (!sendBatch()) {
        delay(2000 * attempts); // backoff
      }
    }
  }

  delay(10000);
}
