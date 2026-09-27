# UNO R4 WiFi Multi-Service Health Checker

Arduino UNO R4 WiFi firmware that monitors several HTTPS resources, sounds a
buzzer when they fail, and exposes its state through a small JSON HTTP API.

## Behaviour

### Health checks

- Each enabled service is checked with an HTTPS `HEAD` request.
- Only HTTP `200` counts as healthy. Any other status, DNS or TLS failure,
  or timeout is an unhealthy check.
- Checks run sequentially, one service at a time, with at least
  `MINIMUM_GAP_BETWEEN_CHECKS_MS` between them.
- A healthy service is rechecked after its `intervalMs`. After an unhealthy
  check it is retried after `FAILURE_RETRY_INTERVAL_MS` (15 s).
- A service is **failing** after `FAILURE_THRESHOLD` (2) consecutive unhealthy
  checks. Only failing services raise the alarm, so a single transient error
  does not.

### Alarm

| Condition | Pattern |
|---|---|
| At least one service failing | two short beeps every 6 s |
| Wi-Fi down for `WIFI_OUTAGE_ALARM_MS` (60 s) | one long beep every 10 s |
| Wi-Fi module missing (fatal) | fast beeping, then reset after 60 s |
| Buzzer test | continuous for 3 s |

- The pattern is generated from a hardware-timer interrupt. It keeps running
  while the main loop is blocked in a DNS lookup or TLS handshake.
- A silenced alarm is automatically re-enabled when a **new** fault appears:
  another service starts failing, or the Wi-Fi outage alarm starts. It is also
  cleared when all faults disappear.

### LED matrix

- While at least one service is failing, the built-in 12 x 8 LED matrix
  shows a steady caution sign: a triangle with an exclamation mark. The
  matrix is off otherwise.
- The sign follows the same rule as the service alarm (`FAILURE_THRESHOLD`
  consecutive failures). It stays on when the buzzer is silenced, so a
  silenced fault remains visible.
- The matrix is refreshed from its own timer interrupt, so the sign stays lit
  during blocking health checks.

### Wi-Fi

- The network counts as connected only when the board has joined the access
  point **and** DHCP has assigned an IP address. The WiFiS3 library reports
  "connected" before the address is known.
- When the address arrives, the serial log prints the SSID, IP address,
  signal strength, and the API endpoints with that address. This repeats
  after every reconnection. `GET /api/status` reports it as `ip_address`.
- If no address arrives within `WIFI_ADDRESS_TIMEOUT_MS` (20 s) after
  joining, the board reconnects.

### Watchdog

- The RA4M1 hardware watchdog is started in `setup()` with a ~5.6 s period,
  which is the hardware maximum.
- A single Wi-Fi module call can legitimately block for up to 10 s (the
  WiFiS3 modem timeout), which is longer than that period. So a 10 Hz timer
  interrupt refreshes the hardware watchdog, but only while the main loop has
  checked in within `LOOP_WATCHDOG_TIMEOUT_MS` (30 s).
- The main loop checks in on every pass and between the steps of a health
  check. A slow or hanging server therefore never resets the board. A stuck
  main loop or Wi-Fi driver resets it within about 36 s. A stuck interrupt
  system resets it within about 5.6 s.
- `GET /api/status` reports `watchdog_timeout_ms`,
  `loop_watchdog_timeout_ms`, and `last_reset_by_watchdog`. After a watchdog
  reset the serial log prints `WARNING: restarted by the watchdog.`
- Service state is not kept across a reset. After a restart, every service is
  checked again from scratch.

## Project files

All sources are in `src/UNO_R4_Health_Checker/`:

- `UNO_R4_Health_Checker.ino`: application entry point.
- `AppConfig.h`: monitored services, timeouts, and hardware constants.
- `ServiceModels.h`: service configuration and runtime-state structures.
- `NetworkManager.*`: Wi-Fi connection, reconnection, and outage tracking.
- `HealthChecker.*`: scheduler and HTTPS `HEAD` checks.
- `AlarmController.*`: timer-driven buzzer patterns.
- `StatusDisplay.*`: caution sign on the built-in LED matrix.
- `Watchdog.*`: hardware watchdog with main-loop supervision.
- `ApiServer.*`: JSON HTTP API.
- `HttpLineReader.*`: bounded, watchdog-aware HTTP line reader.
- `TextParsing.*`: Arduino-independent parsing helpers (unit tested on the
  host).
- `arduino_secrets.h.example`: template for the Wi-Fi credentials and the
  optional API token.

## Setup

1. Copy `arduino_secrets.h.example` to `arduino_secrets.h` in the sketch
   folder. The copy is ignored by git; never commit it.
2. Enter the Wi-Fi SSID and password. Optionally set `SECRET_API_TOKEN` (see
   [Authentication](#authentication)).
3. Edit `SERVICE_CONFIGS` in `AppConfig.h`.
4. Make sure the Wi-Fi module firmware is up to date (Arduino IDE:
   *Tools → Firmware Updater*). The serial log warns when it is older than
   the version the WiFiS3 library expects.
5. Upload the root certificates required by every HTTPS target, if they are
   not already in the module's bundle.

### Build with arduino-cli

```sh
arduino-cli core install arduino:renesas_uno
tools/build.sh                                 # compile
tools/build.sh -p /dev/cu.usbmodemXXXX         # compile and upload
```

### Build with Arduino IDE

Open `src/UNO_R4_Health_Checker/UNO_R4_Health_Checker.ino`.

## Adding services

Edit `SERVICE_CONFIGS` in `AppConfig.h`:

```cpp
const ServiceConfig SERVICE_CONFIGS[] = {
  {
    "service-id",            // id: unique, URL-safe
    "Human-readable name",   // name
    "host.example.com",      // host: no "https://"
    "/health",               // path: begins with "/"
    443,                     // port
    60UL * 1000UL,           // intervalMs
    10000UL,                 // timeoutMs (response wait)
    true                     // enabled
  }
};
```

- All targets use TLS through `WiFiSSLClient`.
- Check that each target answers `HEAD <path>` with `200`. Redirects (`301`,
  `302`) and `405 Method Not Allowed` count as failures. Point `path` at the
  final URL.

## API

All responses are JSON. Only one request is served at a time, and none while
a health check is running (up to about `timeoutMs` + TLS connect time).

| Method | Path | Result |
|---|---|---|
| GET | `/api/status` | Device, Wi-Fi, alarm, and watchdog summary |
| GET | `/api/services` | All services with their state |
| GET | `/api/services/{id}` | One service |
| POST | `/api/check` | Queue all enabled services, `202 Accepted` |
| POST | `/api/services/{id}/check` | Queue one service, `202 Accepted` |
| POST | `/api/buzzer/test` | Sound the buzzer for 3 s |
| POST | `/api/buzzer/silence` | Silence the current alarm |
| POST | `/api/buzzer/unsilence` | Re-enable the alarm sound |

Query strings are ignored. Requests with a request line over 160 characters,
a header line over 256 characters, or more than 32 headers are rejected.

Per-service fields include `healthy` (the last check passed), `failing` (the
service is at or above the alarm threshold), `consecutive_failures`, and
`last_error`. `/api/status` reports `failing_service_count`,
`service_alarm_active`, and `network_alarm_active`.

### Authentication

When `SECRET_API_TOKEN` is set and not empty, every `POST` endpoint requires

```http
Authorization: Bearer <token>
```

and answers `401` otherwise. `GET` endpoints stay open. The API uses plain
HTTP, so the token is visible to anyone who can capture LAN traffic. Do not
expose the device to an untrusted network.

A Postman collection is in `test/`. Set its `baseUrl` and `apiToken`
variables.

## Tests

```sh
test/host/run.sh     # host-side unit tests for TextParsing
tools/build.sh       # firmware compile check
```

See [test/README](test/README) for details.

## Notes

- Times are `millis()` values. The firmware does not use NTP or the RTC.
- The Wi-Fi module's TLS stack does not follow redirects and does not report
  certificate details. A TLS failure shows up as "HTTPS connection failed".

---

&copy; 2017-2026, Dmitry Slobodchikov
