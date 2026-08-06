#include "HealthChecker.h"
#include "AppConfig.h"
#include "WiFiS3.h"

HealthChecker::HealthChecker(
  const ServiceConfig *configs,
  size_t serviceCount,
  AlarmController &alarmController
)
  : _configs(configs),
    _serviceCount(serviceCount),
    _states(nullptr),
    _alarmController(alarmController),
    _roundRobinCursor(0)
{
  _states = new ServiceState[_serviceCount];
}

HealthChecker::~HealthChecker()
{
  delete[] _states;
}

void HealthChecker::begin()
{
  for (size_t i = 0; i < _serviceCount; i++)
  {
    _states[i].checked = false;
    _states[i].healthy = false;
    _states[i].checkQueued = _configs[i].enabled;
    _states[i].checkInProgress = false;

    _states[i].httpStatus = 0;

    _states[i].lastCheckStartedAtMs = 0;
    _states[i].lastCheckCompletedAtMs = 0;
    _states[i].lastCheckDurationMs = 0;
    _states[i].nextCheckAtMs = 0;

    _states[i].successfulChecks = 0;
    _states[i].failedChecks = 0;
    _states[i].consecutiveFailures = 0;

    _states[i].lastError = "Not checked";
  }
}

void HealthChecker::update()
{
  if (_serviceCount == 0)
  {
    return;
  }

  const unsigned long now = millis();

  /*
    Do not start another TLS connection immediately after the previous one.

    The signed subtraction keeps the millis() rollover comparison safe.
  */
  if (static_cast<long>(now - _nextCheckAllowedAtMs) < 0)
  {
    return;
  }

  // Perform no more than one check during one loop iteration.
  for (size_t offset = 0; offset < _serviceCount; offset++)
  {
    const size_t index =
      (_roundRobinCursor + offset) % _serviceCount;

    if (!_configs[index].enabled)
    {
      continue;
    }

    ServiceState &state = _states[index];

    const bool due =
      state.checkQueued ||
      !state.checked ||
      static_cast<long>(now - state.nextCheckAtMs) >= 0;

    if (!due || state.checkInProgress)
    {
      continue;
    }

    state.checkQueued = false;

    _roundRobinCursor =
      (index + 1) % _serviceCount;

    performCheck(index);

    /*
      Give the ESP32-S3 networking module time to close and release the
      previous TLS socket before opening another connection.
    */
    _nextCheckAllowedAtMs =
      millis() + MINIMUM_GAP_BETWEEN_CHECKS_MS;

    return;
  }
}

size_t HealthChecker::serviceCount() const
{
  return _serviceCount;
}

const ServiceConfig &HealthChecker::configAt(size_t index) const
{
  return _configs[index];
}

const ServiceState &HealthChecker::stateAt(size_t index) const
{
  return _states[index];
}

int HealthChecker::findServiceIndex(const String &id) const
{
  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (id == _configs[i].id)
    {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void HealthChecker::queueAll()
{
  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (_configs[i].enabled)
    {
      _states[i].checkQueued = true;
    }
  }
}

bool HealthChecker::queueService(size_t index)
{
  if (index >= _serviceCount || !_configs[index].enabled)
  {
    return false;
  }

  _states[index].checkQueued = true;
  return true;
}

bool HealthChecker::anyServiceUnhealthy() const
{
  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (_configs[i].enabled &&
        _states[i].checked &&
        !_states[i].healthy)
    {
      return true;
    }
  }

  return false;
}

bool HealthChecker::allCheckedServicesHealthy() const
{
  bool foundCheckedService = false;

  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (!_configs[i].enabled || !_states[i].checked)
    {
      continue;
    }

    foundCheckedService = true;

    if (!_states[i].healthy)
    {
      return false;
    }
  }

  return foundCheckedService;
}

size_t HealthChecker::unhealthyServiceCount() const
{
  size_t result = 0;

  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (_configs[i].enabled &&
        _states[i].checked &&
        !_states[i].healthy)
    {
      result++;
    }
  }

  return result;
}

size_t HealthChecker::checkedServiceCount() const
{
  size_t result = 0;

  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (_configs[i].enabled && _states[i].checked)
    {
      result++;
    }
  }

  return result;
}

void HealthChecker::performCheck(size_t index)
{
  const ServiceConfig &config = _configs[index];
  ServiceState &state = _states[index];

  state.checkInProgress = true;
  state.lastCheckStartedAtMs = millis();
  state.httpStatus = 0;
  state.lastError = "";

  Serial.println();
  Serial.print(F("Checking ["));
  Serial.print(config.id);
  Serial.print(F("] https://"));
  Serial.print(config.host);
  Serial.println(config.path);

  bool healthy = false;
  WiFiSSLClient client;

  client.setTimeout(config.timeoutMs);


  IPAddress resolvedIp;

  Serial.print(F("Resolving host: "));
  Serial.println(config.host);

  if (WiFi.hostByName(config.host, resolvedIp) != 1)
  {
    state.lastError = "DNS resolution failed";

    Serial.print(F("DNS resolution failed for: "));
    Serial.println(config.host);
  }
  else
  {
    Serial.print(F("Resolved IP: "));
    Serial.println(resolvedIp);

    Serial.print(F("Opening TLS connection to: "));
    Serial.print(config.host);
    Serial.print(':');
    Serial.println(config.port);

    if (!client.connect(config.host, config.port))
    {
      state.lastError = "HTTPS connection failed";

      Serial.print(F("TLS connection failed for host: "));
      Serial.print(config.host);
      Serial.print(F(", port: "));
      Serial.println(config.port);
    }
    else
    {
      client.print(F("HEAD "));
      client.print(config.path);
      client.println(F(" HTTP/1.1"));

      client.print(F("Host: "));
      client.println(config.host);

      client.println(F("User-Agent: UNO-R4-Health-Checker/2.0"));
      client.println(F("Accept: */*"));
      client.println(F("Connection: close"));
      client.println();

      const unsigned long waitStartedAt = millis();

      while (!client.available() &&
            client.connected() &&
            millis() - waitStartedAt < config.timeoutMs)
      {
        _alarmController.update();
        delay(1);
      }

      if (!client.available())
      {
        state.lastError = "HTTP response timeout";
      }
      else
      {
        String statusLine = client.readStringUntil('\n');
        statusLine.trim();

        Serial.print(F("Response: "));
        Serial.println(statusLine);

        state.httpStatus = parseStatusCode(statusLine);

        if (state.httpStatus <= 0)
        {
          state.lastError = "Invalid HTTP status line";
        }
        else if (state.httpStatus == 200)
        {
          healthy = true;
        }
        else
        {
          state.lastError = "Unexpected HTTP status";
        }

        /*
          Consume the remaining HTTP response headers.

          Even though HEAD responses have no body, leaving unread headers in the
          socket can interfere with closing and recycling the connection.
        */
        const unsigned long headersStartedAt = millis();

        while (millis() - headersStartedAt < config.timeoutMs)
        {
          if (client.available())
          {
            String headerLine = client.readStringUntil('\n');

            if (headerLine == "\r" || headerLine.length() == 0)
            {
              break;
            }
          }
          else if (!client.connected())
          {
            break;
          }
          else
          {
            _alarmController.update();
            delay(1);
          }
        }
      }

      /*
        Ask the WiFi coprocessor to close the TLS connection.
      */
      client.stop();

      /*
        Let commands and socket state propagate to the connectivity module.
        The longer inter-service gap is still enforced by update().
      */
      delay(50);
    }
  }

  state.lastCheckDurationMs = millis() - state.lastCheckStartedAtMs;
  state.lastCheckCompletedAtMs = millis();
  state.nextCheckAtMs =
    state.lastCheckCompletedAtMs + config.intervalMs;

  state.checked = true;
  state.healthy = healthy;
  state.checkInProgress = false;

  if (healthy)
  {
    state.successfulChecks++;
    state.consecutiveFailures = 0;
    state.lastError = "";

    Serial.print(F("Service healthy: "));
    Serial.print(config.id);
    Serial.print(F(", HTTP "));
    Serial.print(state.httpStatus);
  }
  else
  {
    state.failedChecks++;
    state.consecutiveFailures++;

    Serial.print(F("Service unhealthy: "));
    Serial.print(config.id);
    Serial.print(F(", error: "));
    Serial.print(state.lastError);

    if (state.httpStatus > 0)
    {
      Serial.print(F(", HTTP "));
      Serial.print(state.httpStatus);
    }
  }

  Serial.print(F(", duration "));
  Serial.print(state.lastCheckDurationMs);
  Serial.println(F(" ms"));

  refreshAlarmState();
}

int HealthChecker::parseStatusCode(const String &statusLine) const
{
  if (!statusLine.startsWith("HTTP/"))
  {
    return 0;
  }

  const int firstSpace = statusLine.indexOf(' ');

  if (firstSpace < 0 || statusLine.length() < firstSpace + 4)
  {
    return 0;
  }

  return statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
}

void HealthChecker::refreshAlarmState()
{
  _alarmController.setAlarmActive(anyServiceUnhealthy());
}
