#ifndef SERVICE_MODELS_H
#define SERVICE_MODELS_H

#include <Arduino.h>

/**
  * @brief Static description of one monitored HTTPS resource.
  * @param id (const char*) Unique, URL-safe identifier used by the API.
  * @param name (const char*) Human-readable name.
  * @param host (const char*) DNS host name without scheme; also used for SNI.
  * @param path (const char*) Request path beginning with "/".
  * @param port (uint16_t) TCP port, normally 443.
  * @param intervalMs (unsigned long) Time between checks while healthy.
  * @param timeoutMs (unsigned long) Maximum wait for the HTTP response.
  * @param enabled (bool) Whether the service is checked at all.
  */
struct ServiceConfig
{
  const char *id;
  const char *name;
  const char *host;
  const char *path;

  uint16_t port;
  unsigned long intervalMs;
  unsigned long timeoutMs;

  bool enabled;
};

/**
  * @brief Runtime state of one service. All times are millis() values.
  * @param checked (bool) At least one check has completed.
  * @param healthy (bool) The most recent check returned HTTP 200.
  * @param failing (bool) consecutiveFailures reached FAILURE_THRESHOLD; this
  *        is what drives the alarm.
  * @param checkQueued (bool) A check was requested through the API.
  * @param checkInProgress (bool) A check is running now.
  * @param httpStatus (int) Status of the last response, or 0 when none.
  * @param lastCheckStartedAtMs (unsigned long) Start of the last check.
  * @param lastCheckCompletedAtMs (unsigned long) End of the last check.
  * @param lastCheckDurationMs (unsigned long) Duration of the last check.
  * @param nextCheckAtMs (unsigned long) When the next scheduled check is due.
  * @param successfulChecks (unsigned long) Total healthy checks.
  * @param failedChecks (unsigned long) Total unhealthy checks.
  * @param consecutiveFailures (unsigned long) Unhealthy checks since the last
  *        healthy one.
  * @param lastError (const char*) Static description of the last failure, or
  *        an empty string.
  */
struct ServiceState
{
  bool checked;
  bool healthy;
  bool failing;
  bool checkQueued;
  bool checkInProgress;

  int httpStatus;

  unsigned long lastCheckStartedAtMs;
  unsigned long lastCheckCompletedAtMs;
  unsigned long lastCheckDurationMs;
  unsigned long nextCheckAtMs;

  unsigned long successfulChecks;
  unsigned long failedChecks;
  unsigned long consecutiveFailures;

  const char *lastError;
};

#endif
