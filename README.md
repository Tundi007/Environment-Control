# Environment Control Backend

Spring Boot backend for device login, upload orchestration, and batch data ingestion with a minimal web console and Arduino client example.

## Running locally

```bash
cd environment-control
./mvnw spring-boot:run # or mvn spring-boot:run if mvnw unavailable
```

PostgreSQL is used by default. Update `spring.datasource.url`, `spring.datasource.username`, and `spring.datasource.password` in `src/main/resources/application.yml` (or supply them via environment variables) to point at your database instance.

## Device APIs

- `POST /api/devices/login` — JSON body `{ "deviceId", "secret" }` returns a JWT token when credentials match a registered device.
- `GET /api/devices/pending-requests` — Requires `Authorization: Bearer <token>`. Optional `longPoll=true` waits up to ~20s. Responds with `{ "uploadRequested": true|false, "lastSequenceAcknowledged": n }`. Add `acknowledge=true` to clear the flag when retrieved.
- `POST /api/devices/data` — Authenticated batch ingestion. Body `{ "records": [{ "sequenceNumber": 1, "payload": "..."}, ...] }`. Returns `{ "lastProcessedSequence": n }` representing the highest sequence stored.

## Admin/web endpoints

Open `http://localhost:8080/` for the minimal console to:

- Register devices with a device ID, secret, and name.
- Trigger or clear the upload flag per device.
- Inspect stored data and the last acknowledged sequence.

## Arduino client sketch

See [`arduino/DeviceClient.ino`](environment-control/arduino/DeviceClient.ino) for a reference implementation that:

- Connects over HTTPS, logs in with device ID/secret, and caches the returned JWT.
- Stores sensor readings with sequence numbers in EEPROM as a ring buffer.
- Long-polls for upload requests, posts readings in batches with retry/backoff, and advances the send pointer using the server acknowledgment.

Replace Wi-Fi credentials, TLS root certificate, and API host/port before uploading to hardware.

## ESP-01 bridge and satellite sketches

The `arduino` folder also contains a small Wi-Fi bridge plus satellite sketches
for ESP-01 modules so you can keep sensors off-board while still feeding the
Arduino client:

- `ESP01_WifiBridge.ino` — joins your Wi-Fi network and exposes a TCP server on
  port `4210`, forwarding any newline-delimited payload to the Arduino over the
  hardware UART. It also reads newline-delimited messages arriving from the
  Arduino over Serial and POSTs them to a configurable webserver so the
  Arduino can publish data without its own Wi-Fi shield.
- `ESP01_MQ135_Client.ino` — samples the MQ135 gas sensor on pin `A0` and sends
  `MQ135:<value>` lines to the bridge.
- `ESP01_DHT11_Client.ino` — reads a DHT11 on GPIO2 and sends
  `DHT11:<temperatureC>:<humidity>` payloads.
- `ESP01_HYSRF05_Client.ino` — drives an HY-SRF05 ultrasonic sensor on GPIO0
  (TRIG) and GPIO2 (ECHO), sending `HYSRF05:<distance_cm>` to the bridge.

Set the `WIFI_SSID`, `WIFI_PASSWORD`, and `BRIDGE_HOST` constants before
uploading. The bridge sketch should run on the ESP-01 attached to the Arduino
hardware serial port; each satellite sketch runs on its own ESP-01 module that
connects to the bridge over Wi-Fi.

### How the ESP-01 pair talk (no Arduino Cloud required)

- The bridge ESP-01 joins your existing Wi-Fi with the credentials in
  `ESP01_WifiBridge.ino`; there is no Arduino Cloud dependency.
- Wire the bridge RX/TX to your Arduino (e.g., Nano) hardware serial pins with
  proper 3.3V level shifting, and cross RX->TX / TX->RX so the bridge can
  forward data in both directions.
- Each sensor ESP-01 runs its client sketch, joins the same Wi-Fi, opens a TCP
  connection to the bridge at port `4210`, and sends newline-delimited payloads
  (e.g., `DHT11:23.1:44.0`). The bridge prints those lines to the Arduino over
  Serial so your sketch can parse them directly.
- When your Arduino sketch prints a newline-delimited message to Serial, the
  bridge posts it to the configured webserver in `ESP01_WifiBridge.ino`. If you
  do not need upstream posting, you can leave `API_HOST` blank or ignore the
  helper and simply read bridge Serial output on the host PC.
