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
