#include "AppConfig.h"
#include "AlarmController.h"
#include "NetworkManager.h"
#include "HealthChecker.h"
#include "ApiServer.h"
#include "arduino_secrets.h"

AlarmController alarmController(BUZZER_PIN, BUZZER_ACTIVE_HIGH);

NetworkManager networkManager(
  SECRET_SSID,
  SECRET_PASS,
  WIFI_RECONNECT_INTERVAL_MS
);

HealthChecker healthChecker(
  SERVICE_CONFIGS,
  SERVICE_COUNT,
  alarmController
);

ApiServer apiServer(
  API_PORT,
  networkManager,
  healthChecker,
  alarmController
);

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);

  const unsigned long serialStartedAt = millis();
  while (!Serial && millis() - serialStartedAt < SERIAL_WAIT_TIMEOUT_MS)
  {
    // Allow the native USB serial port a short time to appear.
  }

  Serial.println();
  Serial.println(F("UNO R4 WiFi Multi-Service Health Checker"));
  Serial.println(F("========================================="));

  alarmController.begin();

  if (!networkManager.begin())
  {
    Serial.println(F("Fatal error: WiFi module is unavailable."));

    while (true)
    {
      alarmController.setHardwareFaultPattern();
      alarmController.update();
    }
  }

  apiServer.begin();
  healthChecker.begin();

  networkManager.printStatus();
  apiServer.printEndpoints();
}

void loop()
{
  networkManager.update();
  alarmController.update();
  apiServer.update();

  if (networkManager.isConnected())
  {
    healthChecker.update();
  }
}
