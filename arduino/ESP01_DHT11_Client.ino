#include <ESP8266WiFi.h>
#include <DHT.h>

/**
 * ESP-01 sketch for sampling a DHT11 temperature/humidity sensor and sending
 * the reading to the Wi-Fi bridge ESP-01 attached to the Arduino. The message
 * format is "DHT11:<temperatureC>:<humidityPercent>".
 */

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";

const char* BRIDGE_HOST = "192.168.1.50"; // IP of the bridge ESP-01
const uint16_t BRIDGE_PORT = 4210;

const uint8_t DHT_PIN = 2; // GPIO2 on the ESP-01
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  dht.begin();
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    delay(2000);
    return;
  }

  WiFiClient client;
  if (client.connect(BRIDGE_HOST, BRIDGE_PORT)) {
    client.print(String("DHT11:") + temperature + ":" + humidity + "\n");
    client.readStringUntil('\n');
  }
  client.stop();
  delay(5000);
}
