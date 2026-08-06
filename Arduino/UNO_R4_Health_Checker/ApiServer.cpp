#include "ApiServer.h"
#include "AppConfig.h"

ApiServer::ApiServer(
  uint16_t port,
  NetworkManager &networkManager,
  HealthChecker &healthChecker,
  AlarmController &alarmController
)
  : _server(port),
    _networkManager(networkManager),
    _healthChecker(healthChecker),
    _alarmController(alarmController)
{
}

void ApiServer::begin()
{
  _server.begin();
  Serial.println(F("API server started."));
}

void ApiServer::update()
{
  WiFiClient client = _server.available();

  if (!client)
  {
    return;
  }

  handleClient(client);
  client.stop();
}

void ApiServer::printEndpoints() const
{
  const IPAddress ip = _networkManager.localIp();

  Serial.println(F("API endpoints:"));

  Serial.print(F("  GET  http://"));
  Serial.print(ip);
  Serial.println(F("/api/status"));

  Serial.print(F("  GET  http://"));
  Serial.print(ip);
  Serial.println(F("/api/services"));

  Serial.print(F("  GET  http://"));
  Serial.print(ip);
  Serial.println(F("/api/services/{id}"));

  Serial.print(F("  POST http://"));
  Serial.print(ip);
  Serial.println(F("/api/check"));

  Serial.print(F("  POST http://"));
  Serial.print(ip);
  Serial.println(F("/api/services/{id}/check"));

  Serial.print(F("  POST http://"));
  Serial.print(ip);
  Serial.println(F("/api/buzzer/test"));

  Serial.print(F("  POST http://"));
  Serial.print(ip);
  Serial.println(F("/api/buzzer/silence"));

  Serial.print(F("  POST http://"));
  Serial.print(ip);
  Serial.println(F("/api/buzzer/unsilence"));
}

void ApiServer::handleClient(WiFiClient &client)
{
  client.setTimeout(API_CLIENT_TIMEOUT_MS);

  const unsigned long requestStartedAt = millis();

  while (!client.available() &&
         client.connected() &&
         millis() - requestStartedAt < API_CLIENT_TIMEOUT_MS)
  {
    _alarmController.update();
  }

  if (!client.available())
  {
    return;
  }

  String requestLine = client.readStringUntil('\n');
  requestLine.trim();

  // Consume headers. This API currently has no request body.
  while (client.connected())
  {
    if (!client.available())
    {
      break;
    }

    String headerLine = client.readStringUntil('\n');
    headerLine.trim();

    if (headerLine.length() == 0)
    {
      break;
    }
  }

  const int firstSpace = requestLine.indexOf(' ');
  const int secondSpace = requestLine.indexOf(' ', firstSpace + 1);

  if (firstSpace <= 0 || secondSpace <= firstSpace)
  {
    sendError(client, 400, "Bad Request", "Invalid HTTP request");
    return;
  }

  const String method = requestLine.substring(0, firstSpace);
  const String path = requestLine.substring(firstSpace + 1, secondSpace);

  Serial.print(F("API request: "));
  Serial.print(method);
  Serial.print(' ');
  Serial.println(path);

  routeRequest(client, method, path);
}

void ApiServer::routeRequest(
  WiFiClient &client,
  const String &method,
  const String &path
)
{
  if (path == "/api/status")
  {
    if (method != "GET")
    {
      sendError(client, 405, "Method Not Allowed", "Use GET");
      return;
    }

    sendOverview(client);
    return;
  }

  if (path == "/api/services")
  {
    if (method != "GET")
    {
      sendError(client, 405, "Method Not Allowed", "Use GET");
      return;
    }

    sendServices(client);
    return;
  }

  if (path == "/api/check")
  {
    if (method != "POST")
    {
      sendError(client, 405, "Method Not Allowed", "Use POST");
      return;
    }

    _healthChecker.queueAll();

    sendJson(
      client,
      202,
      "Accepted",
      "{\"result\":\"All enabled services queued for checking\"}"
    );
    return;
  }

  if (path == "/api/buzzer/test")
  {
    if (method != "POST")
    {
      sendError(client, 405, "Method Not Allowed", "Use POST");
      return;
    }

    _alarmController.startTest();

    sendJson(
      client,
      200,
      "OK",
      "{\"result\":\"Buzzer test started\"}"
    );
    return;
  }

  if (path == "/api/buzzer/silence")
  {
    if (method != "POST")
    {
      sendError(client, 405, "Method Not Allowed", "Use POST");
      return;
    }

    _alarmController.silence();

    sendJson(
      client,
      200,
      "OK",
      "{\"result\":\"Alarm silenced\"}"
    );
    return;
  }

  if (path == "/api/buzzer/unsilence")
  {
    if (method != "POST")
    {
      sendError(client, 405, "Method Not Allowed", "Use POST");
      return;
    }

    _alarmController.unsilence();

    sendJson(
      client,
      200,
      "OK",
      "{\"result\":\"Alarm enabled\"}"
    );
    return;
  }

  const String prefix = "/api/services/";

  if (path.startsWith(prefix))
  {
    String remainder = path.substring(prefix.length());
    bool checkRequest = false;

    if (remainder.endsWith("/check"))
    {
      checkRequest = true;
      remainder.remove(remainder.length() - 6);
    }

    if (remainder.length() == 0 || remainder.indexOf('/') >= 0)
    {
      sendError(client, 404, "Not Found", "Service endpoint not found");
      return;
    }

    const int serviceIndex = _healthChecker.findServiceIndex(remainder);

    if (serviceIndex < 0)
    {
      sendError(client, 404, "Not Found", "Unknown service id");
      return;
    }

    if (checkRequest)
    {
      if (method != "POST")
      {
        sendError(client, 405, "Method Not Allowed", "Use POST");
        return;
      }

      _healthChecker.queueService(static_cast<size_t>(serviceIndex));

      String json = "{\"result\":\"Service queued for checking\",\"service_id\":\"";
      json += escapeJson(remainder);
      json += "\"}";

      sendJson(client, 202, "Accepted", json);
      return;
    }

    if (method != "GET")
    {
      sendError(client, 405, "Method Not Allowed", "Use GET");
      return;
    }

    sendService(client, static_cast<size_t>(serviceIndex));
    return;
  }

  sendError(client, 404, "Not Found", "API endpoint not found");
}

void ApiServer::sendOverview(WiFiClient &client)
{
  String json;
  json.reserve(500);

  json += "{";
  json += "\"device\":\"UNO R4 WiFi Multi-Service Health Checker\",";
  json += "\"wifi_connected\":";
  json += boolJson(_networkManager.isConnected());
  json += ",";

  json += "\"ip_address\":\"";
  json += _networkManager.localIp().toString();
  json += "\",";

  json += "\"rssi_dbm\":";
  json += String(_networkManager.rssi());
  json += ",";

  json += "\"service_count\":";
  json += String(_healthChecker.serviceCount());
  json += ",";

  json += "\"checked_service_count\":";
  json += String(_healthChecker.checkedServiceCount());
  json += ",";

  json += "\"unhealthy_service_count\":";
  json += String(_healthChecker.unhealthyServiceCount());
  json += ",";

  json += "\"all_checked_services_healthy\":";
  json += boolJson(_healthChecker.allCheckedServicesHealthy());
  json += ",";

  json += "\"alarm_active\":";
  json += boolJson(_alarmController.isAlarmActive());
  json += ",";

  json += "\"buzzer_silenced\":";
  json += boolJson(_alarmController.isSilenced());
  json += ",";

  json += "\"buzzer_test_active\":";
  json += boolJson(_alarmController.isTestActive());
  json += ",";

  json += "\"uptime_ms\":";
  json += String(millis());

  json += "}";

  sendJson(client, 200, "OK", json);
}

void ApiServer::sendServices(WiFiClient &client)
{
  String json;
  json.reserve(700 + _healthChecker.serviceCount() * 450);

  json += "{\"services\":[";

  for (size_t i = 0; i < _healthChecker.serviceCount(); i++)
  {
    if (i > 0)
    {
      json += ",";
    }

    json += buildServiceJson(i);
  }

  json += "]}";

  sendJson(client, 200, "OK", json);
}

void ApiServer::sendService(WiFiClient &client, size_t index)
{
  sendJson(client, 200, "OK", buildServiceJson(index));
}

String ApiServer::buildServiceJson(size_t index) const
{
  const ServiceConfig &config = _healthChecker.configAt(index);
  const ServiceState &state = _healthChecker.stateAt(index);

  String json;
  json.reserve(500);

  json += "{";

  json += "\"id\":\"";
  json += escapeJson(config.id);
  json += "\",";

  json += "\"name\":\"";
  json += escapeJson(config.name);
  json += "\",";

  json += "\"target\":\"https://";
  json += escapeJson(config.host);
  json += escapeJson(config.path);
  json += "\",";

  json += "\"port\":";
  json += String(config.port);
  json += ",";

  json += "\"enabled\":";
  json += boolJson(config.enabled);
  json += ",";

  json += "\"interval_ms\":";
  json += String(config.intervalMs);
  json += ",";

  json += "\"timeout_ms\":";
  json += String(config.timeoutMs);
  json += ",";

  json += "\"checked\":";
  json += boolJson(state.checked);
  json += ",";

  json += "\"healthy\":";
  json += boolJson(state.healthy);
  json += ",";

  json += "\"check_queued\":";
  json += boolJson(state.checkQueued);
  json += ",";

  json += "\"check_in_progress\":";
  json += boolJson(state.checkInProgress);
  json += ",";

  json += "\"http_status\":";
  json += String(state.httpStatus);
  json += ",";

  json += "\"last_error\":\"";
  json += escapeJson(state.lastError);
  json += "\",";

  json += "\"last_check_duration_ms\":";
  json += String(state.lastCheckDurationMs);
  json += ",";

  json += "\"last_check_completed_at_ms\":";
  json += String(state.lastCheckCompletedAtMs);
  json += ",";

  json += "\"next_check_at_ms\":";
  json += String(state.nextCheckAtMs);
  json += ",";

  json += "\"successful_checks\":";
  json += String(state.successfulChecks);
  json += ",";

  json += "\"failed_checks\":";
  json += String(state.failedChecks);
  json += ",";

  json += "\"consecutive_failures\":";
  json += String(state.consecutiveFailures);

  json += "}";

  return json;
}

void ApiServer::sendJson(
  WiFiClient &client,
  int statusCode,
  const char *statusText,
  const String &json
)
{
  client.print(F("HTTP/1.1 "));
  client.print(statusCode);
  client.print(' ');
  client.println(statusText);

  client.println(F("Content-Type: application/json; charset=utf-8"));
  client.println(F("Cache-Control: no-store"));
  client.println(F("Connection: close"));

  client.print(F("Content-Length: "));
  client.println(json.length());

  client.println();
  client.print(json);
}

void ApiServer::sendError(
  WiFiClient &client,
  int statusCode,
  const char *statusText,
  const char *message
)
{
  String json = "{\"error\":\"";
  json += escapeJson(message);
  json += "\"}";

  sendJson(client, statusCode, statusText, json);
}

String ApiServer::escapeJson(const String &value) const
{
  String result;
  result.reserve(value.length() + 8);

  for (unsigned int i = 0; i < value.length(); i++)
  {
    const char c = value.charAt(i);

    switch (c)
    {
      case '"':
        result += "\\\"";
        break;

      case '\\':
        result += "\\\\";
        break;

      case '\n':
        result += "\\n";
        break;

      case '\r':
        result += "\\r";
        break;

      case '\t':
        result += "\\t";
        break;

      default:
        result += c;
        break;
    }
  }

  return result;
}

const char *ApiServer::boolJson(bool value) const
{
  return value ? "true" : "false";
}
