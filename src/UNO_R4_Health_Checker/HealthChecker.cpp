#include "HealthChecker.h"
#include "AppConfig.h"
#include "HttpLineReader.h"
#include "TextParsing.h"
#include "UtcTime.h"
#include "CertificateProbe.h"
#include "WiFiS3.h"

namespace
{
constexpr size_t STATUS_LINE_CAPACITY = 64;
constexpr size_t HEADER_LINE_CAPACITY = 128;
constexpr size_t REQUEST_CAPACITY = 256;

/**
  * @brief Time left of a budget that started at startedAt.
  * @retval (unsigned long) Remaining milliseconds, or 0 when expired.
  */
unsigned long remainingMs(unsigned long startedAt, unsigned long budgetMs)
{
  const unsigned long elapsed = millis() - startedAt;
  return elapsed < budgetMs ? budgetMs - elapsed : 0;
}
}  // namespace

HealthChecker::HealthChecker(
  const ServiceConfig *configs,
  size_t serviceCount,
  AlarmController &alarmController,
  Heartbeat &heartbeat,
  Watchdog &watchdog
)
  : _configs(configs),
    _serviceCount(serviceCount),
    _states(nullptr),
    _alarmController(alarmController),
    _heartbeat(heartbeat),
    _watchdog(watchdog),
    _roundRobinCursor(0),
    _nextCheckAllowedAtMs(0)
{
  _states = new ServiceState[_serviceCount];
}

HealthChecker::~HealthChecker()
{
  delete[] _states;
}

void HealthChecker::begin()
{
  const unsigned long now = millis();

  for (size_t i = 0; i < _serviceCount; i++)
  {
    ServiceState &state = _states[i];

    state.checked = false;
    state.healthy = false;
    state.failing = false;
    state.checkQueued = _configs[i].enabled;
    state.checkInProgress = false;

    state.httpStatus = 0;

    state.lastCheckStartedAtMs = 0;
    state.lastCheckCompletedAtMs = 0;
    state.lastCheckDurationMs = 0;
    state.nextCheckAtMs = now;

    state.successfulChecks = 0;
    state.failedChecks = 0;
    state.consecutiveFailures = 0;

    state.lastError = "Not checked";

    state.certificateKnown = false;
    state.certificateExpiring = false;
    state.certificateCheckQueued = _configs[i].enabled;
    state.certificateNotAfterUnixSeconds = 0;
    state.nextCertificateCheckAtMs = now;
    state.certificateError = "Not checked";
  }

  refreshAlarmState();
}

void HealthChecker::update()
{
  if (_serviceCount == 0)
  {
    return;
  }

  refreshCertificateWarning();

  const unsigned long now = millis();

  // Do not start another TLS connection immediately after the previous one.
  // The signed difference keeps the comparison correct across millis()
  // rollover.
  if (static_cast<long>(now - _nextCheckAllowedAtMs) < 0)
  {
    return;
  }

  // Perform no more than one check per call.
  for (size_t offset = 0; offset < _serviceCount; offset++)
  {
    const size_t index = (_roundRobinCursor + offset) % _serviceCount;

    if (!_configs[index].enabled)
    {
      continue;
    }

    ServiceState &state = _states[index];

    const bool checkDue =
      state.checkQueued ||
      static_cast<long>(now - state.nextCheckAtMs) >= 0;

    const bool certificateCheckDue =
      state.certificateCheckQueued ||
      static_cast<long>(now - state.nextCertificateCheckAtMs) >= 0;

    if ((!checkDue && !certificateCheckDue) || state.checkInProgress)
    {
      continue;
    }

    _roundRobinCursor = (index + 1) % _serviceCount;

    if (checkDue)
    {
      state.checkQueued = false;
      performCheck(index);
    }
    else
    {
      state.certificateCheckQueued = false;
      performCertificateCheck(index);
    }

    // Give the Wi-Fi module time to release the previous TLS socket.
    _nextCheckAllowedAtMs = millis() + MINIMUM_GAP_BETWEEN_CHECKS_MS;
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

int HealthChecker::findServiceIndex(const char *id) const
{
  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (strcmp(id, _configs[i].id) == 0)
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
      _states[i].certificateCheckQueued = true;
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
  _states[index].certificateCheckQueued = true;
  return true;
}

size_t HealthChecker::failingServiceCount() const
{
  size_t result = 0;

  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (_configs[i].enabled && _states[i].failing)
    {
      result++;
    }
  }

  return result;
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

size_t HealthChecker::expiringCertificateCount() const
{
  size_t result = 0;

  for (size_t i = 0; i < _serviceCount; i++)
  {
    if (_configs[i].enabled && _states[i].certificateExpiring)
    {
      result++;
    }
  }

  return result;
}

const UtcClock &HealthChecker::clock() const
{
  return _clock;
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
  _heartbeat.setBusy(true);
  state.httpStatus = 0;
  state.lastError = "";

  Serial.println();
  Serial.print(F("Checking ["));
  Serial.print(config.id);
  Serial.print(F("] https://"));
  Serial.print(config.host);
  Serial.println(config.path);

  const bool healthy = runRequest(config, state);

  _heartbeat.setBusy(false);
  state.lastCheckCompletedAtMs = millis();
  state.lastCheckDurationMs =
    state.lastCheckCompletedAtMs - state.lastCheckStartedAtMs;
  state.checkInProgress = false;

  recordResult(index, healthy);
}

bool HealthChecker::runRequest(const ServiceConfig &config, ServiceState &state)
{
  IPAddress resolvedIp;

  _watchdog.refresh();

  if (WiFi.hostByName(config.host, resolvedIp) != 1)
  {
    state.lastError = "DNS resolution failed";
    return false;
  }

  Serial.print(F("Resolved IP: "));
  Serial.println(resolvedIp);

  WiFiSSLClient client;

  if (TLS_CONNECT_TIMEOUT_MS > 0)
  {
    client.setConnectionTimeout(TLS_CONNECT_TIMEOUT_MS);
  }

  _watchdog.refresh();

  if (!client.connect(config.host, config.port))
  {
    state.lastError = "HTTPS connection failed";

    // connect() allocates a socket on the Wi-Fi module even when it fails,
    // and ~WiFiSSLClient() does not release it.
    _watchdog.refresh();
    client.stop();
    return false;
  }

  _watchdog.refresh();

  // Send the whole request in one write: each write is a separate command
  // to the Wi-Fi module.
  char request[REQUEST_CAPACITY];
  const int requestLength = snprintf(
    request,
    sizeof(request),
    "HEAD %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "User-Agent: UNO-R4-Health-Checker/3.0\r\n"
    "Accept: */*\r\n"
    "Connection: close\r\n"
    "\r\n",
    config.path,
    config.host
  );

  bool healthy = false;

  if (requestLength <= 0 || static_cast<size_t>(requestLength) >= sizeof(request))
  {
    state.lastError = "Request too long";
  }
  else if (client.write(
             reinterpret_cast<const uint8_t *>(request),
             static_cast<size_t>(requestLength)
           ) != static_cast<size_t>(requestLength))
  {
    state.lastError = "HTTPS write failed";
  }
  else
  {
    const unsigned long responseStartedAt = millis();

    char statusLine[STATUS_LINE_CAPACITY];
    const LineReadResult statusResult = readHttpLine(
      client,
      statusLine,
      sizeof(statusLine),
      config.timeoutMs,
      _watchdog
    );

    if (statusResult == LineReadResult::TIMED_OUT)
    {
      state.lastError = "HTTP response timeout";
    }
    else if (statusResult == LineReadResult::CLOSED)
    {
      state.lastError = "Connection closed before response";
    }
    else
    {
      // An overlong status line is truncated; the code is at its start.
      Serial.print(F("Response: "));
      Serial.println(statusLine);

      state.httpStatus = parseHttpStatusCode(statusLine);

      if (state.httpStatus == 0)
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

      // Drain the response headers so the module can close the socket
      // cleanly. HEAD responses have no body.
      char headerLine[HEADER_LINE_CAPACITY];

      while (true)
      {
        const unsigned long budget =
          remainingMs(responseStartedAt, config.timeoutMs);

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
            headerResult == LineReadResult::CLOSED ||
            (headerResult == LineReadResult::COMPLETE && headerLine[0] == '\0'))
        {
          break;
        }

        const char *dateValue = matchHttpHeader(headerLine, "Date");
        uint32_t serverTime = 0;

        if (headerResult == LineReadResult::COMPLETE &&
            dateValue != nullptr &&
            parseHttpDate(dateValue, serverTime))
        {
          _clock.set(serverTime);
        }
      }
    }
  }

  _watchdog.refresh();
  client.stop();

  // Let the close propagate to the module. The longer inter-service gap is
  // enforced by update().
  delay(50);

  return healthy;
}

void HealthChecker::recordResult(size_t index, bool healthy)
{
  const ServiceConfig &config = _configs[index];
  ServiceState &state = _states[index];

  const bool wasFailing = state.failing;

  state.checked = true;
  state.healthy = healthy;

  if (healthy)
  {
    state.successfulChecks++;
    state.consecutiveFailures = 0;
    state.lastError = "";
  }
  else
  {
    state.failedChecks++;
    state.consecutiveFailures++;
  }

  state.failing = state.consecutiveFailures >= FAILURE_THRESHOLD;

  const unsigned long delayMs =
    healthy ? config.intervalMs
            : min(config.intervalMs, FAILURE_RETRY_INTERVAL_MS);

  state.nextCheckAtMs = state.lastCheckCompletedAtMs + delayMs;

  if (healthy)
  {
    Serial.print(F("Service healthy: "));
    Serial.print(config.id);
    Serial.print(F(", HTTP "));
    Serial.print(state.httpStatus);
  }
  else
  {
    Serial.print(F("Service unhealthy: "));
    Serial.print(config.id);
    Serial.print(F(", error: "));
    Serial.print(state.lastError);

    if (state.httpStatus > 0)
    {
      Serial.print(F(", HTTP "));
      Serial.print(state.httpStatus);
    }

    Serial.print(F(", consecutive failures: "));
    Serial.print(state.consecutiveFailures);
  }

  Serial.print(F(", duration "));
  Serial.print(state.lastCheckDurationMs);
  Serial.println(F(" ms"));

  // A service that newly crosses the threshold re-arms a silenced buzzer
  // even when other services are already failing.
  if (state.failing && !wasFailing)
  {
    _alarmController.notifyNewFault();
  }

  refreshAlarmState();
}

void HealthChecker::refreshAlarmState()
{
  _alarmController.setServiceAlarm(failingServiceCount() > 0);
}

void HealthChecker::performCertificateCheck(size_t index)
{
  const ServiceConfig &config = _configs[index];
  ServiceState &state = _states[index];

  state.checkInProgress = true;
  _heartbeat.setBusy(true);

  Serial.println();
  Serial.print(F("Reading certificate ["));
  Serial.print(config.id);
  Serial.print(F("] "));
  Serial.print(config.host);
  Serial.print(F(":"));
  Serial.println(config.port);

  uint32_t notAfter = 0;
  const char *errorText = "";
  const unsigned long startedAt = millis();

  const bool success = readCertificateExpiry(
    config.host,
    config.port,
    config.timeoutMs,
    _watchdog,
    notAfter,
    errorText
  );

  const unsigned long completedAt = millis();

  _heartbeat.setBusy(false);
  state.checkInProgress = false;

  if (success)
  {
    state.certificateKnown = true;
    state.certificateNotAfterUnixSeconds = notAfter;
    state.certificateError = "";
    state.nextCertificateCheckAtMs = completedAt + CERTIFICATE_CHECK_INTERVAL_MS;

    char notAfterText[24];
    formatIsoUtc(notAfter, notAfterText, sizeof(notAfterText));

    Serial.print(F("Certificate expires "));
    Serial.print(notAfterText);

    if (_clock.isSet())
    {
      Serial.print(F(", "));
      Serial.print(daysUntil(notAfter, _clock.nowUnixSeconds()));
      Serial.print(F(" days left"));
    }
  }
  else
  {
    // Keep the last known expiry: a temporary failure must not hide a
    // certificate that is about to expire.
    state.certificateError = errorText;
    state.nextCertificateCheckAtMs = completedAt + CERTIFICATE_RETRY_INTERVAL_MS;

    Serial.print(F("Certificate read failed: "));
    Serial.print(errorText);
  }

  Serial.print(F(", duration "));
  Serial.print(completedAt - startedAt);
  Serial.println(F(" ms"));

  refreshCertificateWarning();
}

void HealthChecker::refreshCertificateWarning()
{
  if (!_clock.isSet())
  {
    return;
  }

  const uint32_t now = _clock.nowUnixSeconds();
  bool anyExpiring = false;

  for (size_t i = 0; i < _serviceCount; i++)
  {
    ServiceState &state = _states[i];

    const bool expiring =
      _configs[i].enabled &&
      state.certificateKnown &&
      daysUntil(state.certificateNotAfterUnixSeconds, now) <
        static_cast<long>(CERTIFICATE_WARNING_DAYS);

    // A certificate that newly enters the window re-arms a silenced buzzer
    // even when another one is already in it.
    if (expiring && !state.certificateExpiring)
    {
      Serial.print(F("Certificate expiring soon: "));
      Serial.println(_configs[i].id);
      _alarmController.notifyNewFault();
    }

    state.certificateExpiring = expiring;
    anyExpiring = anyExpiring || expiring;
  }

  _alarmController.setCertificateWarning(anyExpiring);
}
