#ifndef HEALTH_CHECKER_H
#define HEALTH_CHECKER_H

#include <Arduino.h>
#include "ServiceModels.h"
#include "AlarmController.h"

class HealthChecker
{
public:
  HealthChecker(
    const ServiceConfig *configs,
    size_t serviceCount,
    AlarmController &alarmController
  );

  ~HealthChecker();

  void begin();
  void update();

  size_t serviceCount() const;

  const ServiceConfig &configAt(size_t index) const;
  const ServiceState &stateAt(size_t index) const;

  int findServiceIndex(const String &id) const;

  void queueAll();
  bool queueService(size_t index);

  bool anyServiceUnhealthy() const;
  bool allCheckedServicesHealthy() const;
  size_t unhealthyServiceCount() const;
  size_t checkedServiceCount() const;

private:
  void performCheck(size_t index);
  int parseStatusCode(const String &statusLine) const;
  void refreshAlarmState();

  const ServiceConfig *_configs;
  size_t _serviceCount;

  ServiceState *_states;
  AlarmController &_alarmController;

  size_t _roundRobinCursor;
  unsigned long _nextCheckAllowedAtMs;
};

#endif
