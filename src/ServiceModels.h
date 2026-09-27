#ifndef SERVICE_MODELS_H
#define SERVICE_MODELS_H

#include <Arduino.h>

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

struct ServiceState
{
  bool checked;
  bool healthy;
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

  String lastError;
};

#endif
