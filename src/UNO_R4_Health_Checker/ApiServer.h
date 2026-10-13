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

  Every endpoint is served under /api/v<API_VERSION>/ (see ApiVersion.h).
  Requests for another version are answered with 404.
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
    * @param lastResetByWatchdog (bool) Reported by GET /api/v<N>/status.
    */
  void begin(bool lastResetByWatchdog);

  /**
    * @brief Accept and answer at most one pending request. Blocks for up to
    *        about API_CLIENT_TIMEOUT_MS.
    */
  void update();

  /**
    * @brief Make sure the listener is open and print the endpoints with the
    *        current IP address. Call each time the network becomes ready.
    */
  void onNetworkReady();

  void printEndpoints() const;

private:
  void handleClient(WiFiClient &client);

  void routeRequest(
    WiFiClient &client,
    const char *method,
    const char *path,
    bool authorized
  );

  /**
    * @brief Handle POST <prefix> and POST <prefix>/{number}, which play an
    *        alert test once.
    * @param route (const char*) Request path after the version prefix.
    * @param prefix (const char*) Route of the alert, such as "/buzzer/alert".
    * @param alert (AlertTest) Alert to play.
    * @param isPost (bool) Whether the request method is POST.
    * @retval (bool) False when route does not belong to this alert.
    */
  bool routeAlertTest(
    WiFiClient &client,
    const char *route,
    const char *prefix,
    AlertTest alert,
    bool isPost
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

  // "/api/v<API_VERSION>", without a trailing slash.
  char _apiPrefix[16];
  bool _lastResetByWatchdog;

  NetworkManager &_networkManager;
  HealthChecker &_healthChecker;
  AlarmController &_alarmController;
  Watchdog &_watchdog;
};

#endif
