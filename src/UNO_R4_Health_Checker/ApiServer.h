/**
  ******************************************************************************
  * @file           : ApiServer.h
  * @brief          : JSON HTTP API, versioned under /api/v<N>/.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 05.08.2026
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017-2026 Dmitry Slobodchikov
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

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
    * @param networkManager (NetworkManager&) Source of the Wi-Fi state.
    * @param healthChecker (HealthChecker&) Service states and check queue.
    * @param alarmController (AlarmController&) Buzzer controlled by the API.
    * @param watchdog (Watchdog&) Refreshed while a request is read.
    *        All references must outlive this object.
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

  /**
    * @brief Print every endpoint with the current IP address and API
    *        version to the serial log.
    */
  void printEndpoints() const;

private:
  /**
    * @brief Read one request (request line and headers, bounded in size and
    *        time), check the bearer token, and route it. Blocks for up to
    *        API_CLIENT_TIMEOUT_MS.
    * @param client (WiFiClient&) Connected client; closed by the caller.
    */
  void handleClient(WiFiClient &client);

  /**
    * @brief Dispatch a parsed request to its endpoint and send the response.
    * @param client (WiFiClient&) Client to answer.
    * @param method (const char*) Request method, such as "GET".
    * @param path (const char*) Request path without the query string.
    * @param authorized (bool) Whether the bearer token was valid or none is
    *        required.
    */
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

  /**
    * @brief Answer GET /status with the device summary.
    * @param client (WiFiClient&) Client to answer.
    */
  void sendOverview(WiFiClient &client);

  /**
    * @brief Answer GET /services with every service.
    * @param client (WiFiClient&) Client to answer.
    */
  void sendServices(WiFiClient &client);

  /**
    * @brief Answer GET /services/{id} with one service.
    * @param client (WiFiClient&) Client to answer.
    * @param index (size_t) Valid service index.
    */
  void sendService(WiFiClient &client, size_t index);

  /**
    * @brief Send a complete JSON response and its headers.
    * @param client (WiFiClient&) Client to answer.
    * @param statusCode (int) HTTP status code.
    * @param statusText (const char*) HTTP reason phrase.
    * @param json (const String&) Response body.
    */
  void sendJson(
    WiFiClient &client,
    int statusCode,
    const char *statusText,
    const String &json
  );

  /**
    * @brief Send {"error":"<message>"} with the given status.
    * @param client (WiFiClient&) Client to answer.
    * @param statusCode (int) HTTP status code.
    * @param statusText (const char*) HTTP reason phrase.
    * @param message (const char*) Error description; escaped for JSON.
    */
  void sendError(
    WiFiClient &client,
    int statusCode,
    const char *statusText,
    const char *message
  );

  /**
    * @brief Build the JSON object of one service.
    * @param index (size_t) Valid service index.
    * @retval (String) Service configuration and state as JSON.
    */
  String buildServiceJson(size_t index) const;

  /**
    * @brief Escape a string for use inside a JSON string literal.
    * @param value (const char*) Non-null text.
    * @retval (String) Escaped text, without quotes.
    */
  String escapeJson(const char *value) const;

  /**
    * @brief JSON literal of a boolean.
    * @retval (const char*) "true" or "false".
    */
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
