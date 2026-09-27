#include "AppConfig.h"
#include "AlarmController.h"
#include "NetworkManager.h"
#include "HealthChecker.h"
#include "ApiServer.h"
#include "Watchdog.h"
#include "arduino_secrets.h"

// Optional: define SECRET_API_TOKEN in arduino_secrets.h to require
// "Authorization: Bearer <token>" on every POST endpoint.
#ifndef SECRET_API_TOKEN
#define SECRET_API_TOKEN ""
#endif

Watchdog watchdog;

AlarmController alarmController(BUZZER_PIN, BUZZER_ACTIVE_HIGH);

NetworkManager networkManager(
  SECRET_SSID,
  SECRET_PASS,
  WIFI_RECONNECT_INTERVAL_MS,
  WIFI_CONNECT_TIMEOUT_MS,
  WIFI_ADDRESS_TIMEOUT_MS,
  WIFI_OUTAGE_ALARM_MS
);

HealthChecker healthChecker(
  SERVICE_CONFIGS,
  SERVICE_COUNT,
  alarmController,
  watchdog
);

ApiServer apiServer(
  API_PORT,
  SECRET_API_TOKEN,
  networkManager,
  healthChecker,
  alarmController,
  watchdog
);

/**
  * @brief Sound the fault pattern for HARDWARE_FAULT_RESTART_MS, then reset
  *        the MCU to retry. Never returns.
  */
[[noreturn]] static void haltWithHardwareFault()
{
  alarmController.setHardwareFaultPattern();

  const unsigned long startedAt = millis();

  while (millis() - startedAt < HARDWARE_FAULT_RESTART_MS)
  {
    alarmController.update();
    watchdog.refresh();
    delay(10);
  }

  NVIC_SystemReset();

  while (true)
  {
  }
}

void setup()
{
  const bool watchdogReset = Watchdog::consumeWatchdogResetFlag();

  Serial.begin(SERIAL_BAUD_RATE);

  const unsigned long serialStartedAt = millis();
  while (!Serial && millis() - serialStartedAt < SERIAL_WAIT_TIMEOUT_MS)
  {
    // Allow the native USB serial port a short time to appear.
  }

  Serial.println();
  Serial.println(F("UNO R4 WiFi Multi-Service Health Checker"));
  Serial.println(F("========================================="));

  if (watchdogReset)
  {
    Serial.println(F("WARNING: restarted by the watchdog."));
  }

  if (!alarmController.begin())
  {
    Serial.println(F("WARNING: no hardware timer; buzzer runs from loop()."));
  }

  // Start the watchdog before the first Wi-Fi module call, so a module that
  // never answers resets the board instead of hanging it.
  if (watchdog.begin(WATCHDOG_TIMEOUT_MS, LOOP_WATCHDOG_TIMEOUT_MS))
  {
    Serial.print(F("Watchdog started: hardware "));
    Serial.print(watchdog.hardwareTimeoutMs());
    Serial.print(F(" ms, main loop "));
    Serial.print(watchdog.loopTimeoutMs());
    Serial.println(F(" ms."));

    if (!watchdog.isSupervised())
    {
      Serial.println(F("WARNING: no timer for the watchdog supervisor."));
    }
  }
  else
  {
    Serial.println(F("ERROR: watchdog could not be started."));
  }

  healthChecker.begin();

  if (!networkManager.begin())
  {
    Serial.println(F("Fatal error: WiFi module is unavailable."));
    haltWithHardwareFault();
  }

  watchdog.refresh();

  apiServer.begin(watchdogReset);

  // The IP address and endpoints are printed by loop() once DHCP has
  // assigned an address, which may happen after setup() returns.
}

void loop()
{
  watchdog.refresh();

  networkManager.update();
  alarmController.setNetworkAlarm(networkManager.isOutageAlarmDue());

  if (networkManager.consumeNetworkReady())
  {
    apiServer.onNetworkReady();
  }
  alarmController.update();

  watchdog.refresh();
  apiServer.update();

  if (networkManager.isConnected())
  {
    watchdog.refresh();
    healthChecker.update();
  }
}
