#ifndef HEALTH_CHECKER_H
#define HEALTH_CHECKER_H

#include <Arduino.h>
#include "ServiceModels.h"
#include "AlarmController.h"
#include "Heartbeat.h"
#include "Watchdog.h"

/*
  Sequential HTTPS health-check scheduler.

  update() starts at most one check per call and blocks until that check has
  finished (DNS, TLS connect, request, status line, headers). Each Wi-Fi
  module call is bounded by the WiFiS3 modem timeout (10 s); the watchdog is
  refreshed between those calls and inside the response wait loops. The
  heartbeat LED is steady on while a check runs.
*/
class HealthChecker
{
public:
  /**
    * @param configs (const ServiceConfig*) Non-null service table that
    *        outlives this object.
    * @param serviceCount (size_t) Number of entries in configs.
    * @param alarmController (AlarmController&) Alarm updated after checks.
    * @param heartbeat (Heartbeat&) Marked busy while a check runs.
    * @param watchdog (Watchdog&) Watchdog refreshed during checks.
    */
  HealthChecker(
    const ServiceConfig *configs,
    size_t serviceCount,
    AlarmController &alarmController,
    Heartbeat &heartbeat,
    Watchdog &watchdog
  );

  ~HealthChecker();

  HealthChecker(const HealthChecker &) = delete;
  HealthChecker &operator=(const HealthChecker &) = delete;

  /**
    * @brief Reset all states and queue every enabled service.
    */
  void begin();

  /**
    * @brief Run the next due check, if any. Call only while Wi-Fi is up.
    *        Blocks for the duration of one check.
    */
  void update();

  size_t serviceCount() const;

  const ServiceConfig &configAt(size_t index) const;
  const ServiceState &stateAt(size_t index) const;

  /**
    * @brief Find a service by id.
    * @param id (const char*) Non-null service id.
    * @retval (int) Service index, or -1 when unknown.
    */
  int findServiceIndex(const char *id) const;

  void queueAll();

  /**
    * @brief Request an immediate check of one service.
    * @param index (size_t) Service index.
    * @retval (bool) False when the index is invalid or the service disabled.
    */
  bool queueService(size_t index);

  /**
    * @brief Count enabled services at or above FAILURE_THRESHOLD.
    * @retval (size_t) Number of failing services.
    */
  size_t failingServiceCount() const;

  /**
    * @brief Report whether every checked service passed its last check.
    * @retval (bool) False when no service has been checked yet.
    */
  bool allCheckedServicesHealthy() const;

  size_t checkedServiceCount() const;

private:
  void performCheck(size_t index);

  /**
    * @brief Perform DNS, TLS, request and response steps for one service.
    * @param config (const ServiceConfig&) Service to check.
    * @param state (ServiceState&) Receives httpStatus and lastError.
    * @retval (bool) True when the service answered HTTP 200.
    */
  bool runRequest(const ServiceConfig &config, ServiceState &state);

  /**
    * @brief Update counters and alarm after a check result is known.
    */
  void recordResult(size_t index, bool healthy);

  void refreshAlarmState();

  const ServiceConfig *_configs;
  size_t _serviceCount;

  ServiceState *_states;
  AlarmController &_alarmController;
  Heartbeat &_heartbeat;
  Watchdog &_watchdog;

  size_t _roundRobinCursor;
  unsigned long _nextCheckAllowedAtMs;
};

#endif
