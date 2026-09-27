# UNO R4 WiFi Multi-Service Health Checker

A structured Arduino IDE application for monitoring multiple HTTPS resources.

## Behaviour

- Each enabled service is checked using HTTPS `HEAD`.
- HTTP `200` is healthy.
- Any other HTTP status, TLS/connect failure, or timeout is unhealthy.
- Checks are executed sequentially, one service at a time.
- The buzzer is active while at least one checked service is unhealthy.
- A manually silenced buzzer is automatically re-enabled when a new alarm
  transition occurs.

## Project files

- `UNO_R4_Health_Checker.ino` — application entry point
- `AppConfig.h` — monitored services and hardware/application constants
- `ServiceModels.h` — service configuration and runtime-state structures
- `NetworkManager.*` — Wi-Fi connection and reconnection
- `HealthChecker.*` — scheduler and HTTPS HEAD checks
- `AlarmController.*` — non-blocking buzzer operation
- `ApiServer.*` — JSON HTTP API
- `arduino_secrets.h.example` — Wi-Fi credentials template

## Installation

1. Open `UNO_R4_Health_Checker.ino` in Arduino IDE.
2. Arduino IDE should open all `.h` and `.cpp` files as tabs.
3. Copy `arduino_secrets.h.example` to a new tab/file named
   `arduino_secrets.h`.
4. Enter the Wi-Fi SSID and password.
5. Edit `SERVICE_CONFIGS` in `AppConfig.h`.
6. Upload the root certificates required by every HTTPS target.
7. Compile and upload to Arduino UNO R4 WiFi.

## Adding services

Edit `SERVICE_CONFIGS` in `AppConfig.h`:

```cpp
const ServiceConfig SERVICE_CONFIGS[] = {
  {
    "service-id",
    "Human-readable name",
    "host.example.com",
    "/health",
    443,
    60UL * 1000UL,
    10000UL,
    true
  }
};
```

Rules:

- `id` must be unique and URL-safe.
- `host` must not include `https://`.
- `path` must begin with `/`.
- All targets currently use TLS through `WiFiSSLClient`.
- HTTP `200` is the only healthy response.

## API

### Summary

```http
GET /api/status
```

### All services

```http
GET /api/services
```

### One service

```http
GET /api/services/{id}
```

### Queue all services for immediate checking

```http
POST /api/check
```

Returns `202 Accepted`. Checks are then performed sequentially.

### Queue one service

```http
POST /api/services/{id}/check
```

Returns `202 Accepted`.

### Buzzer

```http
POST /api/buzzer/test
POST /api/buzzer/silence
POST /api/buzzer/unsilence
```

## Notes

The API is intentionally unauthenticated in this development version. Do not
expose it directly to an untrusted network.

The firmware records times as `millis()` values. It does not yet use NTP or a
real-time clock.
