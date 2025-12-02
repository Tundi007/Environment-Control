/**
 * Arduino sketch for boards without Wi-Fi. It consumes sensor readings that
 * arrive over the attached ESP-01 Wi-Fi bridge (see ESP01_WifiBridge.ino),
 * stores them locally in RAM, and mirrors each reading back to the bridge as
 * JSON so it can be posted upstream.
 */
#include <Arduino.h>

struct Reading {
  String source;
  float temperature;
  float humidity;
  uint32_t sequenceNumber;
  unsigned long receivedAtMs;
};

// Metadata to include with each payload
const char* DEVICE_ID = "demo-device";
const size_t MAX_STORED_READINGS = 16;

Reading readings[MAX_STORED_READINGS];
size_t readingCount = 0;
size_t nextInsertIndex = 0;
uint32_t nextSequenceNumber = 0;

void sendReadingToBridge(const Reading& reading) {
  String payload = String("{\"deviceId\":\"") + DEVICE_ID + "\",";
  payload += String("\"sequenceNumber\":") + reading.sequenceNumber + ",";
  payload += String("\"source\":\"") + reading.source + "\",";
  payload += String("\"temperature\":") + reading.temperature + ",";
  payload += String("\"humidity\":") + reading.humidity + "\"}";

  Serial.println(payload); // bridge listens for newline-delimited messages
}

void storeReading(const String& source, float temperature, float humidity) {
  Reading reading;
  reading.source = source;
  reading.temperature = temperature;
  reading.humidity = humidity;
  reading.sequenceNumber = nextSequenceNumber++;
  reading.receivedAtMs = millis();

  readings[nextInsertIndex] = reading;
  if (readingCount < MAX_STORED_READINGS) {
    readingCount++;
  }
  nextInsertIndex = (nextInsertIndex + 1) % MAX_STORED_READINGS;

  sendReadingToBridge(reading);
}

bool parseSensorLine(const String& line, String& source, float& temperature, float& humidity) {
  int firstColon = line.indexOf(':');
  int secondColon = line.indexOf(':', firstColon + 1);
  if (firstColon == -1 || secondColon == -1) {
    return false;
  }

  source = line.substring(0, firstColon);
  temperature = line.substring(firstColon + 1, secondColon).toFloat();
  humidity = line.substring(secondColon + 1).toFloat();
  return true;
}

void handleIncomingSerial() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  String source;
  float temperature = 0.0;
  float humidity = 0.0;
  if (parseSensorLine(line, source, temperature, humidity)) {
    storeReading(source, temperature, humidity);
  }
}

void setup() {
  Serial.begin(115200); // Connected to ESP-01 bridge RX/TX (with level shifting)
}

void loop() {
  handleIncomingSerial();
}
