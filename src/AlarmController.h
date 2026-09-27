#ifndef ALARM_CONTROLLER_H
#define ALARM_CONTROLLER_H

#include <Arduino.h>

class AlarmController
{
public:
  AlarmController(uint8_t pin, bool activeHigh);

  void begin();
  void update();

  void setAlarmActive(bool active);
  bool isAlarmActive() const;

  void silence();
  void unsilence();
  bool isSilenced() const;

  void startTest();
  bool isTestActive() const;

  void setHardwareFaultPattern();

private:
  void writeOutput(bool enabled);

  uint8_t _pin;
  bool _activeHigh;

  bool _alarmActive;
  bool _silenced;
  bool _testActive;
  bool _hardwareFault;

  unsigned long _testStartedAtMs;
};

#endif
