# R4 Health Check Project

## Goal

Connect an Arduino UNO R4 WiFi to a configured wireless network and expose a
minimal HTTP health API.

## WiFi configuration

Copy `src/Srv/WifiModem/wifi_credentials.h.example` to
`src/Srv/WifiModem/wifi_credentials.h`, then set `WIFI_SSID` and
`WIFI_PASSWORD`. The credentials file is ignored by Git.

The network must be reachable by the UNO R4 WiFi's ESP32-S3 co-processor.
After startup, the console prints the assigned IPv4 address and whether the
HTTP listener opened successfully.

## Health API

The server listens on TCP port 80 and exposes one endpoint:

```text
GET /health
```

Test it using the IP address printed on the console:

```sh
curl -i http://<board-ip>/health
```

The successful JSON response is:

```json
{"status":"ok"}
```

All other paths return `404 Not Found` with a JSON error response.

## Tips & Tricks

---

&copy; 2026, Askug Ltd., Dmitry Slobodchikov
