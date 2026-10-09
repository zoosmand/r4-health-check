#include "ApiServer.h"
#include "AppConfig.h"
#include "HttpLineReader.h"
#include "TextParsing.h"
#include "UtcTime.h"

namespace
{
constexpr size_t METHOD_CAPACITY = 8;
constexpr char SERVICES_PREFIX[] = "/api/services/";
constexpr char CHECK_SUFFIX[] = "/check";
constexpr size_t ISO_TIME_CAPACITY = 24;

unsigned long remainingMs(unsigned long startedAt, unsigned long budgetMs)
{
  const unsigned long elapsed = millis() - startedAt;
  return elapsed < budgetMs ? budgetMs - elapsed : 0;
}
}  // namespace

ApiServer::ApiServer(
  uint16_t port,
  const char *apiToken,
  NetworkManager &networkManager,
  HealthChecker &healthChecker,
  AlarmController &alarmController,
  Watchdog &watchdog
)
  : _server(port),
    _apiToken(apiToken),
    _lastResetByWatchdog(false),
    _networkManager(networkManager),
    _healthChecker(healthChecker),
    _alarmController(alarmController),
    _watchdog(watchdog)
{
}

void ApiServer::begin(bool lastResetByWatchdog)
{
  _lastResetByWatchdog = lastResetByWatchdog;

  _server.begin();
  Serial.println(F("API server started."));

  if (_apiToken[0] == '\0')
  {
    Serial.println(F("WARNING: API token not set; POST endpoints are open."));
  }
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

void ApiServer::onNetworkReady()
{
  // No-op when the listener is already open; retries if it failed earlier.
  _server.begin();
  printEndpoints();
}

void ApiServer::printEndpoints() const
{
  static const char *const ENDPOINTS[] = {
    "GET  /api/status",
    "GET  /api/services",
    "GET  /api/services/{id}",
    "POST /api/check",
    "POST /api/services/{id}/check",
    "POST /api/buzzer/test",
    "POST /api/buzzer/silence",
    "POST /api/buzzer/unsilence"
  };

  const IPAddress ip = _networkManager.localIp();

  Serial.println(F("API endpoints:"));

  for (const char *endpoint : ENDPOINTS)
  {
    // "METHOD /path" -> "  METHOD http://<ip>/path"
    const char *path = strchr(endpoint, '/');

    Serial.print(F("  "));
    Serial.write(endpoint, static_cast<size_t>(path - endpoint));
    Serial.print(F("http://"));
    Serial.print(ip);
    Serial.println(path);
  }
}

void ApiServer::handleClient(WiFiClient &client)
{
  const unsigned long startedAt = millis();

  char requestLine[API_MAX_REQUEST_LINE_LENGTH + 1];
  const LineReadResult requestResult = readHttpLine(
    client,
    requestLine,
    sizeof(requestLine),
    API_CLIENT_TIMEOUT_MS,
    _watchdog
  );

  if (requestResult == LineReadResult::TIMED_OUT ||
      requestResult == LineReadResult::CLOSED)
  {
    return;
  }

  if (requestResult == LineReadResult::TRUNCATED)
  {
    sendError(client, 414, "URI Too Long", "Request line too long");
    return;
  }

  // Read the headers; only Authorization is used. This API has no bodies.
  bool authorized = _apiToken[0] == '\0';
  bool headersComplete = false;
  char headerLine[API_MAX_HEADER_LINE_LENGTH + 1];

  for (size_t count = 0; count <= API_MAX_HEADER_COUNT; count++)
  {
    const unsigned long budget = remainingMs(startedAt, API_CLIENT_TIMEOUT_MS);

    if (budget == 0)
    {
      break;
    }

    const LineReadResult headerResult = readHttpLine(
      client,
      headerLine,
      sizeof(headerLine),
      budget,
      _watchdog
    );

    if (headerResult == LineReadResult::TIMED_OUT ||
        headerResult == LineReadResult::CLOSED)
    {
      break;
    }

    if (headerResult == LineReadResult::TRUNCATED)
    {
      sendError(
        client,
        431,
        "Request Header Fields Too Large",
        "Header line too long"
      );
      return;
    }

    if (headerLine[0] == '\0')
    {
      headersComplete = true;
      break;
    }

    const char *authorization = matchHttpHeader(headerLine, "Authorization");

    if (authorization != nullptr &&
        _apiToken[0] != '\0' &&
        strncmp(authorization, "Bearer ", 7) == 0 &&
        constantTimeEquals(_apiToken, authorization + 7))
    {
      authorized = true;
    }
  }

  if (!headersComplete)
  {
    sendError(client, 400, "Bad Request", "Incomplete or oversized headers");
    return;
  }

  char method[METHOD_CAPACITY];
  char path[API_MAX_REQUEST_LINE_LENGTH + 1];

  if (!parseHttpRequestLine(
        requestLine,
        method,
        sizeof(method),
        path,
        sizeof(path)
      ))
  {
    sendError(client, 400, "Bad Request", "Invalid HTTP request");
    return;
  }

  Serial.print(F("API request: "));
  Serial.print(method);
  Serial.print(' ');
  Serial.println(path);

  routeRequest(client, method, path, authorized);
}

void ApiServer::routeRequest(
  WiFiClient &client,
  const char *method,
  const char *path,
  bool authorized
)
{
  const bool isGet = strcmp(method, "GET") == 0;
  const bool isPost = strcmp(method, "POST") == 0;

  // Every state-changing endpoint is a POST, so authorization is checked
  // once here, before routing.
  if (isPost && !authorized)
  {
    sendError(client, 401, "Unauthorized", "Missing or invalid bearer token");
    return;
  }

  if (strcmp(path, "/api/status") == 0)
  {
    if (!isGet)
    {
      sendError(client, 405, "Method Not Allowed", "Use GET");
      return;
    }

    sendOverview(client);
    return;
  }

  if (strcmp(path, "/api/services") == 0)
  {
    if (!isGet)
    {
      sendError(client, 405, "Method Not Allowed", "Use GET");
      return;
    }

    sendServices(client);
    return;
  }

  if (strcmp(path, "/api/check") == 0)
  {
    if (!isPost)
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

  if (strcmp(path, "/api/buzzer/test") == 0)
  {
    if (!isPost)
    {
      sendError(client, 405, "Method Not Allowed", "Use POST");
      return;
    }

    _alarmController.startTest();
    sendJson(client, 200, "OK", "{\"result\":\"Buzzer test started\"}");
    return;
  }

  if (strcmp(path, "/api/buzzer/silence") == 0)
  {
    if (!isPost)
    {
      sendError(client, 405, "Method Not Allowed", "Use POST");
      return;
    }

    _alarmController.silence();
    sendJson(client, 200, "OK", "{\"result\":\"Alarm silenced\"}");
    return;
  }

  if (strcmp(path, "/api/buzzer/unsilence") == 0)
  {
    if (!isPost)
    {
      sendError(client, 405, "Method Not Allowed", "Use POST");
      return;
    }

    _alarmController.unsilence();
    sendJson(client, 200, "OK", "{\"result\":\"Alarm enabled\"}");
    return;
  }

  const size_t prefixLength = sizeof(SERVICES_PREFIX) - 1;

  if (strncmp(path, SERVICES_PREFIX, prefixLength) == 0)
  {
    char serviceId[API_MAX_REQUEST_LINE_LENGTH + 1];
    strncpy(serviceId, path + prefixLength, sizeof(serviceId) - 1);
    serviceId[sizeof(serviceId) - 1] = '\0';

    const size_t suffixLength = sizeof(CHECK_SUFFIX) - 1;
    size_t idLength = strlen(serviceId);
    bool checkRequest = false;

    if (idLength > suffixLength &&
        strcmp(serviceId + idLength - suffixLength, CHECK_SUFFIX) == 0)
    {
      checkRequest = true;
      idLength -= suffixLength;
      serviceId[idLength] = '\0';
    }

    if (idLength == 0 || strchr(serviceId, '/') != nullptr)
    {
      sendError(client, 404, "Not Found", "Service endpoint not found");
      return;
    }

    const int serviceIndex = _healthChecker.findServiceIndex(serviceId);

    if (serviceIndex < 0)
    {
      sendError(client, 404, "Not Found", "Unknown service id");
      return;
    }

    if (checkRequest)
    {
      if (!isPost)
      {
        sendError(client, 405, "Method Not Allowed", "Use POST");
        return;
      }

      if (!_healthChecker.queueService(static_cast<size_t>(serviceIndex)))
      {
        sendError(client, 409, "Conflict", "Service is disabled");
        return;
      }

      String json = "{\"result\":\"Service queued for checking\",\"service_id\":\"";
      json += escapeJson(serviceId);
      json += "\"}";

      sendJson(client, 202, "Accepted", json);
      return;
    }

    if (!isGet)
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
  json.reserve(800);

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

  json += "\"failing_service_count\":";
  json += String(_healthChecker.failingServiceCount());
  json += ",";

  json += "\"all_checked_services_healthy\":";
  json += boolJson(_healthChecker.allCheckedServicesHealthy());
  json += ",";

  json += "\"alarm_active\":";
  json += boolJson(_alarmController.isAlarmActive());
  json += ",";

  json += "\"service_alarm_active\":";
  json += boolJson(_alarmController.isServiceAlarmActive());
  json += ",";

  json += "\"network_alarm_active\":";
  json += boolJson(_alarmController.isNetworkAlarmActive());
  json += ",";

  json += "\"certificate_warning_active\":";
  json += boolJson(_alarmController.isCertificateWarningActive());
  json += ",";

  json += "\"expiring_certificate_count\":";
  json += String(_healthChecker.expiringCertificateCount());
  json += ",";

  // Empty until a health-check response has carried an HTTP Date header.
  char utcTime[ISO_TIME_CAPACITY] = "";

  if (_healthChecker.clock().isSet())
  {
    formatIsoUtc(_healthChecker.clock().nowUnixSeconds(), utcTime, sizeof(utcTime));
  }

  json += "\"utc_time\":\"";
  json += utcTime;
  json += "\",";

  json += "\"buzzer_silenced\":";
  json += boolJson(_alarmController.isSilenced());
  json += ",";

  json += "\"buzzer_test_active\":";
  json += boolJson(_alarmController.isTestActive());
  json += ",";

  json += "\"watchdog_timeout_ms\":";
  json += String(_watchdog.hardwareTimeoutMs());
  json += ",";

  json += "\"loop_watchdog_timeout_ms\":";
  json += String(_watchdog.loopTimeoutMs());
  json += ",";

  json += "\"last_reset_by_watchdog\":";
  json += boolJson(_lastResetByWatchdog);
  json += ",";

  json += "\"uptime_ms\":";
  json += String(millis());

  json += "}";

  sendJson(client, 200, "OK", json);
}

void ApiServer::sendServices(WiFiClient &client)
{
  String json;
  json.reserve(16 + _healthChecker.serviceCount() * 760);

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
  json.reserve(760);

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

  json += "\"failing\":";
  json += boolJson(state.failing);
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
  json += ",";

  char notAfter[ISO_TIME_CAPACITY] = "";

  if (state.certificateKnown)
  {
    formatIsoUtc(state.certificateNotAfterUnixSeconds, notAfter, sizeof(notAfter));
  }

  json += "\"certificate_not_after\":\"";
  json += notAfter;
  json += "\",";

  // null while the expiry or the current time is unknown.
  json += "\"certificate_days_left\":";

  if (state.certificateKnown && _healthChecker.clock().isSet())
  {
    json += String(daysUntil(
      state.certificateNotAfterUnixSeconds,
      _healthChecker.clock().nowUnixSeconds()
    ));
  }
  else
  {
    json += "null";
  }

  json += ",";

  json += "\"certificate_expiring\":";
  json += boolJson(state.certificateExpiring);
  json += ",";

  json += "\"certificate_error\":\"";
  json += escapeJson(state.certificateError);
  json += "\",";

  json += "\"next_certificate_check_at_ms\":";
  json += String(state.nextCertificateCheckAtMs);

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
  // Build the header block in one buffer: every client write is a separate
  // Wi-Fi module command.
  char header[192];
  const int headerLength = snprintf(
    header,
    sizeof(header),
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n"
    "%s"
    "Content-Length: %u\r\n"
    "\r\n",
    statusCode,
    statusText,
    statusCode == 401 ? "WWW-Authenticate: Bearer\r\n" : "",
    json.length()
  );

  if (headerLength > 0 && static_cast<size_t>(headerLength) < sizeof(header))
  {
    client.write(
      reinterpret_cast<const uint8_t *>(header),
      static_cast<size_t>(headerLength)
    );
    client.write(
      reinterpret_cast<const uint8_t *>(json.c_str()),
      json.length()
    );
  }
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

String ApiServer::escapeJson(const char *value) const
{
  String result;
  result.reserve(strlen(value) + 8);

  char escaped[7];

  for (const char *c = value; *c != '\0'; c++)
  {
    escapeJsonChar(*c, escaped);
    result += escaped;
  }

  return result;
}

const char *ApiServer::boolJson(bool value) const
{
  return value ? "true" : "false";
}
