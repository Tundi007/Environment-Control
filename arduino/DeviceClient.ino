/**
 * Arduino sketch for boards without Wi-Fi. It samples readings and streams
 * newline-delimited JSON to an attached ESP-01 Wi-Fi bridge over Serial. The
 * bridge (see ESP01_WifiBridge.ino) posts each line upstream so the Arduino
 * never needs TLS or Wi-Fi libraries.
 */
#include <Arduino.h>

// Metadata to include with each payload
const char* DEVICE_ID = "demo-device";

// Sequence counter persisted in RAM only; adjust if you need EEPROM durability
uint32_t sequenceNumber = 0;

// Helper to format and emit one reading to the ESP-01 bridge
void sendReading(float temperature, float humidity) {
  String payload = String("{\"deviceId\":\"") + DEVICE_ID + "\",";
  payload += String("\"sequenceNumber\":") + sequenceNumber + ",";
  payload += String("\"payload\":\"") + temperature + "," + humidity + "\"}";

  Serial.println(payload); // bridge listens for newline-delimited messages
  sequenceNumber++;
}

void setup() {
  Serial.begin(115200); // Connected to ESP-01 bridge RX/TX (with level shifting)
}

void loop() {
  // Replace with your own sensor sampling logic
  float temperature = random(200, 300) / 10.0; // 20.0–30.0 C
  float humidity = random(300, 500) / 10.0;    // 30.0–50.0 %

  sendReading(temperature, humidity);

  delay(10000); // send every 10 seconds
}
