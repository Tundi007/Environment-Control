/**
 * ESP32 NodeMCU client for the Environment Control backend.
 *
 * - Samples MQ135 (gas), DHT11 (temp/humidity), and HY-SRF05 (ultrasonic).
 * - Buffers readings with sequence numbers in EEPROM (ring buffer).
 * - Posts batches to /api/devices/data using DeviceDataRecord payload strings.
 * - Uses /api/devices/login to obtain a JWT and includes it in uploads.
 *
 * Required libraries (Arduino IDE Library Manager):
 *   - ArduinoJson
 *   - DHT sensor library (by Adafruit)
 *   - MQUnifiedsensor
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <DHT.h>
#include <WebServer.h>
#include <NewPing.h>
#include <MQUnifiedsensor.h>

// ---- Hardware configuration ----
const uint8_t MQ135_PIN = 34;           // ADC pin for MQ135 sensor
const char* MQ135_BOARD = "ESP-32";     // Board identifier for MQUnifiedsensor
const float MQ135_VOLTAGE = 3.3;        // ESP32 ADC reference voltage
const uint16_t MQ135_ADC_RESOLUTION = 12;
const float MQ135_CLEAN_AIR_RATIO = 3.6; // Datasheet recommended clean-air ratio

const uint8_t DHT_PIN = 4;    // GPIO connected to DHT11 data pin
const uint8_t DHT_TYPE = DHT11;

const uint8_t HYSRF_TRIG_PIN = 5;  // GPIO to drive HY-SRF05 trigger
const uint8_t HYSRF_ECHO_PIN = 18; // GPIO to read HY-SRF05 echo
const uint16_t ULTRASONIC_MAX_DISTANCE_CM = 400; // Maximum distance to measure

const uint8_t LED_OUTPUT_PIN = 2;  // On-board LED for remote control output
const uint8_t LED_PWM_CHANNEL = 0;
const uint16_t LED_PWM_FREQUENCY = 5000;
const uint8_t LED_PWM_RESOLUTION = 8; // 0-255 brightness

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

// configuration.
const uint32_t SAMPLE_INTERVAL_MS_DEMO = 3000;  // Add a reading every 1s
const uint32_t UPLOAD_INTERVAL_MS_DEMO = 30000; 
const uint32_t SAMPLE_INTERVAL_MS = 600000;  // Add a reading every 10m
const uint32_t UPLOAD_INTERVAL_MS = 3600000;
const bool ONLY_UPLOAD_WHEN_REQUESTED = true; // true = honor /pending-requests flag
const size_t BATCH_SIZE = 100000000;          // Max records per POST
const bool ENABLE_HTTP_DATA_ENDPOINT = true;  // expose GET /data for admin "Refresh"
const uint16_t DATA_HTTP_PORT = 80;
const bool ENABLE_REMOTE_CONTROL_ENDPOINT = true; // expose POST /remote-control for brightness commands

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
uint8_t ledBrightness = 64; // starting PWM duty cycle (0-255)

MQUnifiedsensor mq135(MQ135_BOARD, MQ135_VOLTAGE, MQ135_ADC_RESOLUTION, MQ135_PIN, "MQ-135");
DHT dht(DHT_PIN, DHT_TYPE);
NewPing sonar(HYSRF_TRIG_PIN, HYSRF_ECHO_PIN, ULTRASONIC_MAX_DISTANCE_CM);

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

String valueOrNan(float value, uint8_t decimals) {
  if (isnan(value)) return String("nan");
  return String(value, static_cast<unsigned int>(decimals));
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

void applyLedOutput() {
  ledcWrite(LED_PWM_CHANNEL, ledBrightness);
}

void adjustLedOutput(int8_t delta) {
  int16_t updated = static_cast<int16_t>(ledBrightness) + delta;
  if (updated < 0) updated = 0;
  if (updated > 255) updated = 255;
  ledBrightness = static_cast<uint8_t>(updated);
  applyLedOutput();
}

String extractCommand(String rawBody) {
  rawBody.toLowerCase();
  if (rawBody.indexOf("increase") >= 0) return "increase";
  if (rawBody.indexOf("decrease") >= 0) return "decrease";
  return "";
}

void initializeSensors() {
  mq135.setRegressionMethod(1); // ppm = a*ratio^b
  mq135.setA(110.47);
  mq135.setB(-2.862);
  mq135.init();

  float r0 = 0;
  const uint8_t samples = 5;
  for (uint8_t i = 0; i < samples; i++) {
    mq135.update();
    r0 += mq135.calibrate(MQ135_CLEAN_AIR_RATIO);
    delay(200);
  }
  r0 /= samples;
  if (isnan(r0) || r0 <= 0) {
    Serial.println("MQ135 calibration failed; using default R0");
    r0 = 10; // fallback value to keep readings finite
  }
  mq135.setR0(r0);

  dht.begin();
}

bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("wifi connected");
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
  String payload = "mq135=" + valueOrNan(r.mq135, 2);
  payload += ",tempC=" + valueOrNan(r.temperatureC, 1);
  payload += ",humidity=" + valueOrNan(r.humidity, 1);
  payload += ",distanceCm=" + valueOrNan(r.distanceCm, 1);
  return payload;
}

bool sendBatch() {
  Serial.println("Transmition Attempt");
  if (sendIndex >= writeIndex) {
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
  Serial.printf("Upload complete: %d\n", code);
  return true;
}

// ---- Local HTTP endpoint for admin console "Refresh" ----
void handleRemoteControl() {
  if (!ENABLE_REMOTE_CONTROL_ENDPOINT) {
    server.send(404, "text/plain", "disabled");
    return;
  }

  String command = server.hasArg("command") ? server.arg("command") : "";
  if (command.isEmpty()) {
    command = extractCommand(server.arg("plain"));
  }

  command.toLowerCase();
  if (command == "increase") {
    adjustLedOutput(25);
  } else if (command == "decrease") {
    adjustLedOutput(-25);
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Unknown command\"}");
    return;
  }

  String response = "{\"status\":\"ok\",\"brightness\":" + String(ledBrightness) + "}";
  server.send(200, "application/json", response);
}

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
float sampleDistanceCm() {
  unsigned long distance = sonar.ping_cm();
  if (distance == 0) {
    return NAN; // out of range
  }
  return static_cast<float>(distance);
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
  mq135.update();
  float mqPpm = mq135.readSensor();

  float temperatureC = dht.readTemperature();
  float humidity = dht.readHumidity();

  float distanceCm = sampleDistanceCm();

  saveReading(mqPpm, temperatureC, humidity, distanceCm);

  String logLine = "Sampled mq135=" + valueOrNan(mqPpm, 2) + "ppm";
  logLine += " tempC=" + valueOrNan(temperatureC, 1);
  logLine += " humidity=" + valueOrNan(humidity, 1) + "%";
  logLine += " distance=" + valueOrNan(distanceCm, 1) + "cm";
  Serial.println(logLine);
}

// ---- Arduino lifecycle ----
unsigned long lastSampleMs = 0;
unsigned long lastUploadMs = 0;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_BYTES);
  loadIndexes();
  initializeSensors();
  pinMode(HYSRF_TRIG_PIN, OUTPUT);
  pinMode(HYSRF_ECHO_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);
  ledcSetup(LED_PWM_CHANNEL, LED_PWM_FREQUENCY, LED_PWM_RESOLUTION);
  ledcAttachPin(LED_OUTPUT_PIN, LED_PWM_CHANNEL);
  applyLedOutput();

  if (ensureWifi() == true) {
    Serial.println("wifi connected");
  } else {
    Serial.println("wifi didn't connect");
  }
  ensureAuthenticated();

  if (ENABLE_HTTP_DATA_ENDPOINT) {
    server.on("/data", handleDataEndpoint);
  }
  if (ENABLE_REMOTE_CONTROL_ENDPOINT) {
    server.on("/remote-control", HTTP_POST, handleRemoteControl);
  }
  if (ENABLE_HTTP_DATA_ENDPOINT || ENABLE_REMOTE_CONTROL_ENDPOINT) {
    server.begin();
  }
}

void loop() {
  const unsigned long now = millis();

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS_DEMO) {
    sampleAndStore();
    lastSampleMs = now;
  }
  

  if (sendIndex < writeIndex && (pollForUpload() || (now - lastUploadMs >= UPLOAD_INTERVAL_MS_DEMO))) {
    if (sendBatch()) {
      lastUploadMs = now;
    }
  }

  server.handleClient();
  delay(50);
}
