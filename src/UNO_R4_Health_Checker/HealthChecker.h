/**
  ******************************************************************************
  * @file           : HealthChecker.h
  * @brief          : Scheduler for HTTPS health checks and certificate reads.
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

#ifndef HEALTH_CHECKER_H
#define HEALTH_CHECKER_H

#include <Arduino.h>
#include "ServiceModels.h"
#include "AlarmController.h"
#include "Heartbeat.h"
#include "Watchdog.h"
#include "UtcClock.h"

/*
  Sequential HTTPS health-check and certificate-expiry scheduler.

  update() starts at most one job per call and blocks until it has finished:
  either a health check (DNS, TLS connect, request, status line, headers) or
  a certificate read (TCP connect, ClientHello, Certificate message; see
  CertificateProbe.h). Health checks take precedence for the same service,
  and a failing service's certificate reads are paused until it recovers.
  Each Wi-Fi module call is bounded by the WiFiS3 modem timeout (10 s); the
  watchdog is refreshed between those calls and inside the response wait
  loops. The heartbeat LED is steady on while a job runs.

  The HTTP Date header of every response sets the UTC clock, which turns the
  certificate expiry dates into the certificate warning.
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

  /**
    * @brief Release the service state table.
    */
  ~HealthChecker();

  HealthChecker(const HealthChecker &) = delete;
  HealthChecker &operator=(const HealthChecker &) = delete;

  /**
    * @brief Reset all states and queue every enabled service.
    */
  void begin();

  /**
    * @brief Refresh the certificate warning, then run the next due health
    *        check or certificate read, if any. Call only while Wi-Fi is up.
    *        Blocks for the duration of one job.
    */
  void update();

  /**
    * @brief Number of configured services, enabled or not.
    */
  size_t serviceCount() const;

  /**
    * @brief Configuration of one service.
    * @param index (size_t) Index below serviceCount().
    * @retval (const ServiceConfig&) Entry of the service table.
    */
  const ServiceConfig &configAt(size_t index) const;

  /**
    * @brief Runtime state of one service.
    * @param index (size_t) Index below serviceCount().
    * @retval (const ServiceState&) State, valid for the object's lifetime.
    */
  const ServiceState &stateAt(size_t index) const;

  /**
    * @brief Find a service by id.
    * @param id (const char*) Non-null service id.
    * @retval (int) Service index, or -1 when unknown.
    */
  int findServiceIndex(const char *id) const;

  /**
    * @brief Request an immediate health check and certificate read of every
    *        enabled service.
    */
  void queueAll();

  /**
    * @brief Request an immediate health check and certificate read of one
    *        service. The certificate read waits while this service is
    *        failing.
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
    * @brief Resource numbers of the enabled services at or above
    *        FAILURE_THRESHOLD.
    * @retval (uint8_t) Mask; bit (n - 1) stands for resource n.
    */
  uint8_t failingResourceMask() const;

  /**
    * @brief Report whether every checked service passed its last check.
    * @retval (bool) False when no service has been checked yet.
    */
  bool allCheckedServicesHealthy() const;

  /**
    * @brief Count enabled services with at least one completed check.
    */
  size_t checkedServiceCount() const;

  /**
    * @brief Count enabled services whose certificate is in the warning
    *        window.
    */
  size_t expiringCertificateCount() const;

  /**
    * @brief Resource numbers of the enabled services whose certificate is in
    *        the warning window.
    * @retval (uint8_t) Mask; bit (n - 1) stands for resource n.
    */
  uint8_t expiringCertificateMask() const;

  /**
    * @brief Clock set from the HTTP Date headers of the health checks.
    */
  const UtcClock &clock() const;

private:
  /**
    * @brief Run one health check, record its result, and log it. Blocks for
    *        the duration of the check.
    * @param index (size_t) Index of an enabled service.
    */
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

  /**
    * @brief Pass the failing resources to the alarm controller.
    */
  void refreshAlarmState();

  /**
    * @brief Read and record the certificate expiry of one service.
    */
  void performCertificateCheck(size_t index);

  /**
    * @brief Recompute certificateExpiring for every service from the clock
    *        and update the alarm controller. Does nothing until the clock
    *        is set.
    */
  void refreshCertificateWarning();

  const ServiceConfig *_configs;
  size_t _serviceCount;

  ServiceState *_states;
  AlarmController &_alarmController;
  Heartbeat &_heartbeat;
  Watchdog &_watchdog;

  UtcClock _clock;

  size_t _roundRobinCursor;
  unsigned long _nextCheckAllowedAtMs;
};

#endif
