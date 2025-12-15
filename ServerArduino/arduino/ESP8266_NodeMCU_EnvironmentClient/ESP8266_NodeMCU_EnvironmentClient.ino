/**
 * ESP8266 NodeMCU client for the Environment Control backend.
 *
 * - Samples MQ135 (gas), DHT11 (temp/humidity), and HY-SRF05 (ultrasonic).
 * - Packages readings with ISO-8601 timestamps into DeviceDataRecord payload strings.
 * - Posts batches to /api/devices/data using the same payload format as the ESP32 client.
 * - Logs in via /api/devices/login before uploads.
 *
 * Required libraries (Arduino IDE Library Manager):
 *   - ArduinoJson
 *   - DHT sensor library (by Adafruit)
 *   - MQUnifiedsensor
 *   - NewPing
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <DHT.h>
#include <NewPing.h>
#include <MQUnifiedsensor.h>
#include <time.h>

// ---- Hardware configuration ----
const uint8_t MQ135_PIN = A0;           // ADC pin for MQ135 sensor
const char* MQ135_BOARD = "ESP8266";    // Board identifier for MQUnifiedsensor
const float MQ135_VOLTAGE = 3.3;        // ESP8266 ADC reference voltage
const uint16_t MQ135_ADC_RESOLUTION = 10;
const float MQ135_CLEAN_AIR_RATIO = 3.6; // Datasheet recommended clean-air ratio

const uint8_t DHT_PIN = D2;    // GPIO connected to DHT11 data pin
const uint8_t DHT_TYPE = DHT11;

const uint8_t HYSRF_TRIG_PIN = D5;  // GPIO to drive HY-SRF05 trigger
const uint8_t HYSRF_ECHO_PIN = D6;  // GPIO to read HY-SRF05 echo
const uint16_t ULTRASONIC_MAX_DISTANCE_CM = 400; // Maximum distance to measure

// ---- User configuration ----
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";
const char* DEVICE_ID = "esp8266";
const char* DEVICE_SECRET = "changeme";
const char* API_HOST = "192.168.1.2";
const uint16_t API_PORT = 8080; // 8443 for HTTPS

// Set to true and provide ROOT_CA when your API uses HTTPS.
#define USE_TLS false
#if USE_TLS
static const char* ROOT_CA =
    "-----BEGIN CERTIFICATE-----\n"
    "...\n"
    "-----END CERTIFICATE-----\n";
#endif

// ---- Timing configuration ----
const uint32_t SAMPLE_INTERVAL_MS = 600000; // 10 minutes

// ---- Internal state ----
struct Reading {
  uint32_t sequence;
  float mq135;
  float temperatureC;
  float humidity;
  float distanceCm;
  time_t timestamp;
};

MQUnifiedsensor mq135(MQ135_BOARD, MQ135_VOLTAGE, MQ135_ADC_RESOLUTION, MQ135_PIN, "MQ-135");
DHT dht(DHT_PIN, DHT_TYPE);
NewPing sonar(HYSRF_TRIG_PIN, HYSRF_ECHO_PIN, ULTRASONIC_MAX_DISTANCE_CM);

#if USE_TLS
BearSSL::WiFiClientSecure netClient;
#else
WiFiClient netClient;
#endif

String jwtToken;
uint32_t sequenceNumber = 0;
unsigned long lastSampleMillis = 0;

// ---- Utility helpers ----
String baseUrl() {
  String proto = USE_TLS ? "https://" : "http://";
  return proto + String(API_HOST) + ":" + String(API_PORT);
}

String valueOrNan(float value, uint8_t decimals) {
  if (isnan(value)) return String("nan");
  return String(value, static_cast<unsigned int>(decimals));
}

String formatTimestamp(time_t ts) {
  struct tm timeinfo;
  gmtime_r(&ts, &timeinfo);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buffer);
}

bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

bool syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 20; i++) {
    time_t now = time(nullptr);
    if (now > 100000) {
      return true;
    }
    delay(500);
  }
  return false;
}

bool login() {
  if (!ensureWifi()) return false;

#if USE_TLS
  netClient.setTrustAnchors(new BearSSL::X509List(ROOT_CA));
#endif

  HTTPClient http;
  const String url = baseUrl() + "/api/devices/login";
  http.begin(netClient, url);
  http.addHeader("Content-Type", "application/json");
  const String body =
      "{\"deviceId\":\"" + String(DEVICE_ID) + "\",\"deviceSecret\":\"" + String(DEVICE_SECRET) + "\"}";
  int code = http.POST(body);
  if (code != HTTP_CODE_OK) {
    Serial.printf("Login failed: %d\n", code);
    http.end();
    return false;
  }
  String response = http.getString();
  http.end();

  int tokenIndex = response.indexOf("token\":\"");
  if (tokenIndex < 0) return false;
  int start = tokenIndex + 8;
  int end = response.indexOf('"', start);
  if (end < 0) return false;
  jwtToken = response.substring(start, end);
  return !jwtToken.isEmpty();
}

bool ensureAuthenticated() {
  if (!jwtToken.isEmpty()) return true;
  return login();
}

Reading sampleSensors() {
  Reading r{};
  r.sequence = sequenceNumber++;
  r.timestamp = time(nullptr);

  mq135.update();
  r.mq135 = mq135.readSensor();

  r.humidity = dht.readHumidity();
  r.temperatureC = dht.readTemperature();

  unsigned int uS = sonar.ping();
  r.distanceCm = sonar.convert_cm(uS);
  return r;
}

String makePayload(const Reading& r) {
  // String payload matches DeviceDataRecord.payload expected by the backend.
  String payload = "timestamp=" + formatTimestamp(r.timestamp);
  payload += ",mq135=" + valueOrNan(r.mq135, 2);
  payload += ",tempC=" + valueOrNan(r.temperatureC, 1);
  payload += ",humidity=" + valueOrNan(r.humidity, 1);
  payload += ",distanceCm=" + valueOrNan(r.distanceCm, 1);
  return payload;
}

bool sendReading(const Reading& r) {
  if (!ensureAuthenticated()) return false;

  HTTPClient http;
  const String url = baseUrl() + "/api/devices/data";
  http.begin(netClient, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);

  String body = "{\"records\":[";
  body += "{\"sequenceNumber\":" + String(r.sequence) + ",\"payload\":\"" + makePayload(r) + "\"}";
  body += "]}";

  int code = http.POST(body);
  if (code != HTTP_CODE_OK) {
    Serial.printf("Upload failed: %d\n", code);
    http.end();
    return false;
  }
  http.end();
  Serial.printf("Uploaded sequence %lu\n", static_cast<unsigned long>(r.sequence));
  return true;
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

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("ESP8266 Environment Client starting...");

  if (!ensureWifi()) {
    Serial.println("Wi-Fi connection failed");
  }

  if (!syncTime()) {
    Serial.println("Time sync failed; timestamps may be incorrect");
  }

  initializeSensors();
  login();
  lastSampleMillis = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleMillis >= SAMPLE_INTERVAL_MS || now < lastSampleMillis) {
    lastSampleMillis = now;
    Reading reading = sampleSensors();
    sendReading(reading);
  }
  delay(1000);
}
