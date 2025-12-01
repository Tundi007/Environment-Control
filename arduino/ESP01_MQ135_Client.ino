#include <ESP8266WiFi.h>

/**
 * ESP-01 sketch for sampling an MQ135 gas sensor and sending the reading to
 * the Wi-Fi bridge ESP-01 attached to the Arduino. The message format is a
 * single line: "MQ135:<rawAnalogValue>".
 */

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";

const char* BRIDGE_HOST = "192.168.1.50"; // IP of the bridge ESP-01
const uint16_t BRIDGE_PORT = 4210;

const uint8_t MQ135_PIN = A0;

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void loop() {
  int raw = analogRead(MQ135_PIN);
  WiFiClient client;
  if (client.connect(BRIDGE_HOST, BRIDGE_PORT)) {
    client.print(String("MQ135:") + raw + "\n");
    client.readStringUntil('\n'); // read "OK"
  }
  client.stop();
  delay(5000);
}
