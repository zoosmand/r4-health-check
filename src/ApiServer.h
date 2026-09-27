#ifndef API_SERVER_H
#define API_SERVER_H

#include <Arduino.h>
#include "WiFiS3.h"
#include "NetworkManager.h"
#include "HealthChecker.h"
#include "AlarmController.h"
#include "Watchdog.h"

/*
  Minimal JSON HTTP API. Serves one client per update() call; each request
  is bounded in size (AppConfig.h API_MAX_*) and in time
  (API_CLIENT_TIMEOUT_MS). The API is not served while a health check runs.
*/
class ApiServer
{
public:
  /**
    * @param port (uint16_t) TCP port to listen on.
    * @param apiToken (const char*) Non-null bearer token required for POST
    *        endpoints; an empty string disables authentication. Must outlive
    *        this object.
    */
  ApiServer(
    uint16_t port,
    const char *apiToken,
    NetworkManager &networkManager,
    HealthChecker &healthChecker,
    AlarmController &alarmController,
    Watchdog &watchdog
  );

  /**
    * @brief Start listening.
    * @param lastResetByWatchdog (bool) Reported by GET /api/status.
    */
  void begin(bool lastResetByWatchdog);

  /**
    * @brief Accept and answer at most one pending request. Blocks for up to
    *        about API_CLIENT_TIMEOUT_MS.
    */
  void update();

  void printEndpoints() const;

private:
  void handleClient(WiFiClient &client);

  void routeRequest(
    WiFiClient &client,
    const char *method,
    const char *path,
    bool authorized
  );

  void sendOverview(WiFiClient &client);
  void sendServices(WiFiClient &client);
  void sendService(WiFiClient &client, size_t index);

  void sendJson(
    WiFiClient &client,
    int statusCode,
    const char *statusText,
    const String &json
  );

  void sendError(
    WiFiClient &client,
    int statusCode,
    const char *statusText,
    const char *message
  );

  String buildServiceJson(size_t index) const;
  String escapeJson(const char *value) const;
  const char *boolJson(bool value) const;

  WiFiServer _server;
  const char *_apiToken;
  bool _lastResetByWatchdog;

  NetworkManager &_networkManager;
  HealthChecker &_healthChecker;
  AlarmController &_alarmController;
  Watchdog &_watchdog;
};

#endif
