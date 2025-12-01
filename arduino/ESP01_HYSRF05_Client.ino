#include <ESP8266WiFi.h>

/**
 * ESP-01 sketch for reading an HY-SRF05 ultrasonic sensor and sending the
 * distance measurement to the Wi-Fi bridge ESP-01 attached to the Arduino.
 * The message format is "HYSRF05:<distance_cm>".
 */

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";

const char* BRIDGE_HOST = "192.168.1.50"; // IP of the bridge ESP-01
const uint16_t BRIDGE_PORT = 4210;

const uint8_t TRIG_PIN = 0; // GPIO0 on ESP-01
const uint8_t ECHO_PIN = 2; // GPIO2 on ESP-01

long measureDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout ~5m
  if (duration == 0) {
    return -1;
  }
  return duration / 58; // convert to centimeters
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void loop() {
  long distance = measureDistanceCm();
  if (distance > 0) {
    WiFiClient client;
    if (client.connect(BRIDGE_HOST, BRIDGE_PORT)) {
      client.print(String("HYSRF05:") + distance + "\n");
      client.readStringUntil('\n');
    }
    client.stop();
  }
  delay(1000);
}
