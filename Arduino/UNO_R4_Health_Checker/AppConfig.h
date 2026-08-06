#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <Arduino.h>
#include "ServiceModels.h"

// -----------------------------------------------------------------------------
// General
// -----------------------------------------------------------------------------

constexpr unsigned long SERIAL_BAUD_RATE = 115200UL;
constexpr unsigned long SERIAL_WAIT_TIMEOUT_MS = 3000UL;

// -----------------------------------------------------------------------------
// API server
// -----------------------------------------------------------------------------

constexpr uint16_t API_PORT = 80;
constexpr unsigned long API_CLIENT_TIMEOUT_MS = 1500UL;

// Allow the WiFi coprocessor time to release the previous TLS connection.
constexpr unsigned long MINIMUM_GAP_BETWEEN_CHECKS_MS = 2000UL;
// -----------------------------------------------------------------------------
// Wi-Fi
// -----------------------------------------------------------------------------

constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000UL;

// -----------------------------------------------------------------------------
// Buzzer
// -----------------------------------------------------------------------------

constexpr uint8_t BUZZER_PIN = 8;
constexpr bool BUZZER_ACTIVE_HIGH = true;

constexpr unsigned long BUZZER_TEST_DURATION_MS = 3000UL;

// Alarm pattern:
// 0-500 ms ON
// 500-1000 ms OFF
// 1000-1500 ms ON
// 1500-6000 ms OFF
constexpr unsigned long ALARM_PATTERN_PERIOD_MS = 6000UL;

// -----------------------------------------------------------------------------
// Health-check defaults
// -----------------------------------------------------------------------------

constexpr unsigned long DEFAULT_CHECK_INTERVAL_MS = 60UL * 1000UL;
constexpr unsigned long DEFAULT_HTTP_TIMEOUT_MS = 100000UL;

// Add or remove entries here.
//
// Important:
//   - host must not contain "https://"
//   - path must begin with "/"
//   - each id must be unique and URL-safe
//
// The UNO R4 WiFi certificate store must contain the root CA required by each
// HTTPS host.

const ServiceConfig SERVICE_CONFIGS[] = {
  {
    "secure",
    "Intraclear Old Acquring",
    "secure.intraclear.com",
    "/",
    443,
    DEFAULT_CHECK_INTERVAL_MS,
    DEFAULT_HTTP_TIMEOUT_MS,
    true
  },
  {
    "public-site",
    "Public Website",
    "www.neuro-ural.ru",
    "/",
    443,
    DEFAULT_CHECK_INTERVAL_MS,
    DEFAULT_HTTP_TIMEOUT_MS,
    true
  },
  {
    "pgw-ic",
    "Intraclear New Acquring",
    "pgw.intraclear.com",
    "/",
    443,
    DEFAULT_CHECK_INTERVAL_MS,
    DEFAULT_HTTP_TIMEOUT_MS,
    true
  },
  {
    "pgw-ac",
    "Whitelebled Acquring",
    "pgw.anycrypto.io",
    "/",
    443,
    DEFAULT_CHECK_INTERVAL_MS,
    DEFAULT_HTTP_TIMEOUT_MS,
    true
  }
};

constexpr size_t SERVICE_COUNT =
  sizeof(SERVICE_CONFIGS) / sizeof(SERVICE_CONFIGS[0]);

#endif
