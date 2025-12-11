/**
 * ESP32 NodeMCU client for the Environment Control backend.
 *
 * - Samples MQ135 (analog), DHT11 (temp/humidity), and HY-SRF05 (ultrasonic).
 * - Buffers readings with sequence numbers in EEPROM (ring buffer).
 * - Posts batches to /api/devices/data using the server's DeviceDataRecord
 *   structure: [{ "sequenceNumber": n, "payload": "<string>" }].
 * - Uses /api/devices/login to obtain a JWT and includes it in uploads.
 *
 * Required libraries (Arduino IDE Library Manager):
 *   - ArduinoJson
 *   - DHT sensor library (by Adafruit)
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <DHT.h>
#include <WebServer.h>

// ---- User configuration ----
const char* WIFI_SSID = "Arman";
const char* WIFI_PASSWORD = "2apple3657";
const bool USE_S_IP = true; // set true to force S_IP below
IPAddress S_IP(192, 168, 1, 5);
IPAddress S_GATEWAY(192, 168, 1, 1);
const char* DEVICE_ID = "esp32";
const char* DEVICE_SECRET = "apple32";
const char* API_HOST = "192.168.1.2";
const uint16_t API_PORT = 8080; // 8443 for HTTPS

// Set to true and provide ROOT_CA when your API uses HTTPS.
#define USE_TLS false
#if USE_TLS
const char* ROOT_CA =
    "-----BEGIN CERTIFICATE-----\n"
    "...\n"
    "-----END CERTIFICATE-----\n";
#endif

// Sensor pins (change to match your wiring).
const int MQ135_PIN = 34;       // Analog input (ESP32 ADC pin)
const int DHT_PIN = 4;          // DHT11 data pin
const int HYSRF_TRIG_PIN = 5;   // HY-SRF05 trigger pin
const int HYSRF_ECHO_PIN = 18;  // HY-SRF05 echo pin

// Timing configuration.
const uint32_t SAMPLE_INTERVAL_MS = 10000;  // Add a reading every 10s
const uint32_t UPLOAD_INTERVAL_MS = 2000;   // Try to upload every 2s
const bool ONLY_UPLOAD_WHEN_REQUESTED = true; // true = honor /pending-requests flag
const size_t BATCH_SIZE = 10;               // Max records per POST
const bool ENABLE_HTTP_DATA_ENDPOINT = true; // expose GET /data for admin "Refresh"
const uint16_t DATA_HTTP_PORT = 80;

// ---- Internal state ----
struct Reading {
  uint32_t sequence;
  float mq135;
  float temperatureC;
  float humidity;
  float distanceCm;
};

const size_t MAX_RECORDS = 64;
const size_t EEPROM_HEADER_BYTES = sizeof(uint32_t) * 2; // writeIndex + sendIndex
const size_t EEPROM_BYTES = EEPROM_HEADER_BYTES + MAX_RECORDS * sizeof(Reading);

uint32_t writeIndex = 0;
uint32_t sendIndex = 0;
String jwtToken;

DHT dht(DHT_PIN, DHT11);
#if USE_TLS
WiFiClientSecure netClient;
#else
WiFiClient netClient;
#endif
WebServer server(DATA_HTTP_PORT);

// ---- Utility helpers ----
String baseUrl() {
  return String(USE_TLS ? "https://" : "http://") + API_HOST + ":" + API_PORT;
}

size_t recordAddress(uint32_t index) {
  return EEPROM_HEADER_BYTES + (index % MAX_RECORDS) * sizeof(Reading);
}

void persistIndexes() {
  EEPROM.put(0, writeIndex);
  EEPROM.put(sizeof(uint32_t), sendIndex);
  EEPROM.commit();
}

void loadIndexes() {
  EEPROM.get(0, writeIndex);
  EEPROM.get(sizeof(uint32_t), sendIndex);
  if (writeIndex == 0xFFFFFFFF || writeIndex > 10000000UL) {
    writeIndex = 0;
    sendIndex = 0;
  }
}

bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  if (USE_S_IP) {
    WiFi.config(S_IP, S_GATEWAY);
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

bool login() {
  if (!ensureWifi()) return false;

  #if USE_TLS
    netClient.setCACert(ROOT_CA);
  #endif

  HTTPClient http;
  const String url = baseUrl() + "/api/devices/login";
  String json = String("{\"deviceId\":\"") + DEVICE_ID + "\",\"secret\":\"" + DEVICE_SECRET + "\"}";

  http.begin(netClient, url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);

  if (code != HTTP_CODE_OK) {
    Serial.printf("Login failed: %d\n", code);
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  int tokenIndex = response.indexOf("\"token\":\"");
  if (tokenIndex < 0) {
    Serial.println("No token in response");
    return false;
  }
  int start = tokenIndex + 9; // length of "\"token\":\""
  int end = response.indexOf('\"', start);
  if (end < 0) {
    Serial.println("Token parse failed");
    return false;
  }
  jwtToken = response.substring(start, end);
  Serial.println("Login OK");
  return true;
}

bool ensureAuthenticated() {
  if (!jwtToken.isEmpty()) {
    return true;
  }
  return login();
}

bool pollForUpload() {
  if (!ONLY_UPLOAD_WHEN_REQUESTED) return true;
  if (!ensureAuthenticated()) return false;
  HTTPClient http;
  const String url = baseUrl() + "/api/devices/pending-requests?longPoll=true&acknowledge=true";
  http.begin(netClient, url);
  http.addHeader("Authorization", "Bearer " + jwtToken);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    Serial.println("Payload transmition failed");    
    login();
    return false;
  }
  String body = http.getString();
  http.end();

  int flagIndex = body.indexOf("uploadRequested");
  if (flagIndex < 0) return false;
  int trueIndex = body.indexOf("true", flagIndex);
  return trueIndex > flagIndex;
}

String makePayload(const Reading& r) {
  // String payload matches DeviceDataRecord.payload expected by the backend.
  String payload = "mq135=" + String(r.mq135, 2);
  payload += ",tempC=" + String(r.temperatureC, 1);
  payload += ",humidity=" + String(r.humidity, 1);
  payload += ",distanceCm=" + String(r.distanceCm, 1);
  return payload;
}

bool sendBatch() {
  Serial.println("Transmition Attempt");
  if (sendIndex >= writeIndex){    
    Serial.println("No Data");
    return true;
  } // nothing to send
  if (!ensureAuthenticated()) return false;

  HTTPClient http;
  const String url = baseUrl() + "/api/devices/data";
  http.begin(netClient, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);

  String body = "{\"records\":[";
  uint32_t cursor = sendIndex;
  size_t added = 0;
  for (; cursor < writeIndex && added < BATCH_SIZE; cursor++) {
    Reading r;
    EEPROM.get(recordAddress(cursor), r);
    if (added > 0) body += ",";
    body += "{\"sequenceNumber\":" + String(r.sequence) + ",\"payload\":\"" + makePayload(r) + "\"}";
    added++;
  }
  body += "]}";

  int code = http.POST(body);
  if (code != HTTP_CODE_OK) {
    Serial.printf("Upload failed: %d\n", code);
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  int idx = response.indexOf("lastProcessedSequence");
  if (idx < 0) {
    Serial.println("Ack parse failed");
    return false;
  }
  int colon = response.indexOf(':', idx);
  if (colon < 0) return false;
  uint32_t acked = response.substring(colon + 1).toInt();
  if (acked >= sendIndex && acked < writeIndex) {
    sendIndex = acked + 1;
    persistIndexes();
  }
  return true;
}

// ---- Local HTTP endpoint for admin console "Refresh" ----
void handleDataEndpoint() {
  if (!ENABLE_HTTP_DATA_ENDPOINT) {
    server.send(404, "text/plain", "disabled");
    return;
  }

  String body = "[";
  size_t emitted = 0;
  uint32_t start = sendIndex;
  uint32_t end = writeIndex;
  for (uint32_t i = start; i < end; i++) {
    Reading r;
    EEPROM.get(recordAddress(i), r);
    if (emitted++ > 0) body += ",";
    body += "{\"sequenceNumber\":" + String(r.sequence) + ",\"payload\":\"" + makePayload(r) + "\"}";
  }
  body += "]";
  server.send(200, "application/json", body);
}

// ---- Sensor sampling ----
float sampleMQ135() {
  int raw = analogRead(MQ135_PIN);
  // Convert to voltage (approximate; adjust for your board's reference).
  return (raw / 4095.0f) * 3.3f;
}

float sampleDistanceCm() {
  digitalWrite(HYSRF_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HYSRF_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HYSRF_TRIG_PIN, LOW);
  unsigned long duration = pulseIn(HYSRF_ECHO_PIN, HIGH, 30000); // 30 ms timeout
  if (duration == 0) return NAN;
  return duration / 58.2f; // sound speed conversion to cm
}

void saveReading(float mq135, float tempC, float humidity, float distanceCm) {
  if (writeIndex - sendIndex >= MAX_RECORDS) {
    // Prevent overwriting unsent data by advancing the send cursor.
    sendIndex = writeIndex - MAX_RECORDS + 1;
  }
  Reading r{writeIndex, mq135, tempC, humidity, distanceCm};
  EEPROM.put(recordAddress(writeIndex), r);
  writeIndex++;
  persistIndexes();
}

void sampleAndStore() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (isnan(temp) || isnan(hum)) {
    Serial.println("DHT read failed");
    temp=-300;
    hum=-300;
  }
  float gas = sampleMQ135();
  if (isnan(gas)) {
    Serial.println("MQ135 read failed");
    gas=-300;
  }
  
  float distance = sampleDistanceCm();
  if (isnan(distance)) {
    Serial.println("Distance read failed");
    distance =-300;
  }
  saveReading(gas, temp, hum, distance);
  Serial.printf("Buffered seq %lu | mq135=%.2fV temp=%.1fC hum=%.1f%% dist=%.1fcm\n",
                (unsigned long)(writeIndex - 1), gas, temp, hum, distance);
}

// ---- Arduino lifecycle ----
unsigned long lastSampleMs = 0;
unsigned long lastUploadMs = 0;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_BYTES);
  loadIndexes();
  dht.begin();
  pinMode(HYSRF_TRIG_PIN, OUTPUT);
  pinMode(HYSRF_ECHO_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  if (ensureWifi()==true) { 
    Serial.println("wifi connected");
  }else {
    Serial.println("wifi didn't connect");
  }
  ensureAuthenticated();

  if (ENABLE_HTTP_DATA_ENDPOINT) {
    server.on("/data", handleDataEndpoint);
    server.begin();
  }
}

void loop() {
  
  const unsigned long now = millis();

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    sampleAndStore();
    lastSampleMs = now;
  }

  if(ensureWifi()){
    Serial.println("online");
    if(ensureAuthenticated()){
      Serial.println("authenticated");
      if (sendIndex < writeIndex && now - lastUploadMs >= UPLOAD_INTERVAL_MS) {
        if (pollForUpload()) {
          sendBatch();
        }
        lastUploadMs = now;
      }
    }else {
      Serial.println("Couldn't authenticate");
    }
  }else {    
    Serial.println("offline");
  }

  server.handleClient();
  delay(1000);
}
