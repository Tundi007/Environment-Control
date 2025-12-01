#include <ESP8266WiFi.h>

/**
 * ESP-01 bridge sketch that sits between Wi-Fi-only sensors and an Arduino
 * without Wi-Fi. It exposes a TCP server so ESP-01 sensor clients can send
 * newline-delimited readings that are forwarded to the Arduino over the
 * hardware serial pins. It also reads newline-delimited payloads coming from
 * the Arduino over Serial and forwards them to a webserver as HTTP POST
 * requests, so the Arduino can publish data without a Wi-Fi shield. There is
 * no Arduino Cloud dependency—just point the ESP-01 at your LAN Wi-Fi.
 */

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";

// TCP server configuration for sensor clients
const uint16_t BRIDGE_PORT = 4210;
WiFiServer bridgeServer(BRIDGE_PORT);

// HTTP endpoint that the Arduino wants to post to
const char* API_HOST = "your.api.host"; // IP or hostname of your webserver
const uint16_t API_PORT = 80;            // change to 443 for HTTPS with WiFiClientSecure
const char* API_PATH = "/api/readings"; // path that accepts plain-text payloads

void connectWifi() {
  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void setup() {
  Serial.begin(9600); // Connected to the Arduino's serial pins
  connectWifi();
  bridgeServer.begin();
}

void forwardSensorClients() {
  WiFiClient client = bridgeServer.available();
  if (!client) return;

  // Block until a full line is received from sensor client
  String payload = client.readStringUntil('\n');
  if (payload.length() > 0) {
    Serial.println(payload); // Forward to Arduino
    client.print("OK\n");
  }

  client.stop();
}

void postArduinoMessage(const String& message) {
  if (API_HOST == nullptr || API_HOST[0] == '\0') {
    return; // upstream posting disabled
  }
  WiFiClient webClient;
  if (!webClient.connect(API_HOST, API_PORT)) {
    return; // drop silently if offline
  }

  String body = message + "\n"; // send as plain text
  webClient.print(String("POST ") + API_PATH + " HTTP/1.1\r\n");
  webClient.print(String("Host: ") + API_HOST + "\r\n");
  webClient.print("Content-Type: text/plain\r\n");
  webClient.print(String("Content-Length: ") + body.length() + "\r\n\r\n");
  webClient.print(body);
  webClient.flush();
  delay(10); // brief wait to allow transmit
  webClient.stop();
}

void forwardArduinoToWeb() {
  if (!Serial.available()) return;

  String message = Serial.readStringUntil('\n');
  message.trim();
  if (message.length() == 0) return;

  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }
  postArduinoMessage(message);
}

void loop() {
  forwardSensorClients();
  forwardArduinoToWeb();
}
