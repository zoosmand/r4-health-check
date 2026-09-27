#ifndef API_SERVER_H
#define API_SERVER_H

#include <Arduino.h>
#include "WiFiS3.h"
#include "NetworkManager.h"
#include "HealthChecker.h"
#include "AlarmController.h"

class ApiServer
{
public:
  ApiServer(
    uint16_t port,
    NetworkManager &networkManager,
    HealthChecker &healthChecker,
    AlarmController &alarmController
  );

  void begin();
  void update();
  void printEndpoints() const;

private:
  void handleClient(WiFiClient &client);

  void routeRequest(
    WiFiClient &client,
    const String &method,
    const String &path
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
  String escapeJson(const String &value) const;
  const char *boolJson(bool value) const;

  WiFiServer _server;

  NetworkManager &_networkManager;
  HealthChecker &_healthChecker;
  AlarmController &_alarmController;
};

#endif
