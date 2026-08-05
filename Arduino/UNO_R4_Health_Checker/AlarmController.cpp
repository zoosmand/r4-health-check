#include "AlarmController.h"
#include "AppConfig.h"

AlarmController::AlarmController(uint8_t pin, bool activeHigh)
  : _pin(pin),
    _activeHigh(activeHigh),
    _alarmActive(false),
    _silenced(false),
    _testActive(false),
    _hardwareFault(false),
    _testStartedAtMs(0)
{
}

void AlarmController::begin()
{
  pinMode(_pin, OUTPUT);
  writeOutput(false);
}

void AlarmController::update()
{
  const unsigned long now = millis();

  if (_hardwareFault)
  {
    writeOutput((now % 500UL) < 250UL);
    return;
  }

  if (_testActive)
  {
    if (now - _testStartedAtMs < BUZZER_TEST_DURATION_MS)
    {
      writeOutput(true);
      return;
    }

    _testActive = false;
    writeOutput(false);
  }

  if (!_alarmActive || _silenced)
  {
    writeOutput(false);
    return;
  }

  const unsigned long position = now % ALARM_PATTERN_PERIOD_MS;

  const bool enabled =
    position < 500UL ||
    (position >= 1000UL && position < 1500UL);

  writeOutput(enabled);
}

void AlarmController::setAlarmActive(bool active)
{
  const bool risingEdge = active && !_alarmActive;
  _alarmActive = active;

  // A new alarm condition re-enables audible notification.
  if (risingEdge)
  {
    _silenced = false;
  }

  if (!active)
  {
    _silenced = false;
    writeOutput(false);
  }
}

bool AlarmController::isAlarmActive() const
{
  return _alarmActive;
}

void AlarmController::silence()
{
  _silenced = true;
  writeOutput(false);
}

void AlarmController::unsilence()
{
  _silenced = false;
}

bool AlarmController::isSilenced() const
{
  return _silenced;
}

void AlarmController::startTest()
{
  _testActive = true;
  _testStartedAtMs = millis();
}

bool AlarmController::isTestActive() const
{
  return _testActive;
}

void AlarmController::setHardwareFaultPattern()
{
  _hardwareFault = true;
}

void AlarmController::writeOutput(bool enabled)
{
  const uint8_t activeLevel = _activeHigh ? HIGH : LOW;
  const uint8_t inactiveLevel = _activeHigh ? LOW : HIGH;

  digitalWrite(_pin, enabled ? activeLevel : inactiveLevel);
}
