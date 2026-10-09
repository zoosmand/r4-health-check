#include "AlarmController.h"
#include "AppConfig.h"

AlarmController::AlarmController(uint8_t pin, float toneHz)
  : _pin(pin),
    _toneHz(toneHz),
    _pwm(pin),
    _toneReady(false),
    _toneDutyCounts(0),
    _outputOn(false),
    _timerRunning(false),
    _serviceAlarm(false),
    _networkAlarm(false),
    _silenced(false),
    _testActive(false),
    _hardwareFault(false),
    _testStartedAtMs(0)
{
}

bool AlarmController::begin()
{
  // Hold the pin low until the PWM takes it over.
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);

  // Start at 50% so the period is known, then silence at once.
  _toneReady = _pwm.begin(_toneHz, 50.0f);

  if (_toneReady)
  {
    _toneDutyCounts = _pwm.get_timer()->get_period_raw() / 2U;
    _outputOn = true;
    writeOutput(false);
  }

  uint8_t timerType = 0;
  const int8_t channel = FspTimer::get_available_timer(timerType);

  if (channel < 0)
  {
    return false;
  }

  // The interrupt priority (12) is below the AGT tick used by millis(), so
  // millis() keeps advancing while the callback runs.
  _timerRunning =
    _timer.begin(
      TIMER_MODE_PERIODIC,
      timerType,
      static_cast<uint8_t>(channel),
      BUZZER_TICK_HZ,
      0.0f,
      timerCallback,
      this
    ) &&
    _timer.setup_overflow_irq() &&
    _timer.open() &&
    _timer.start();

  return _timerRunning;
}

void AlarmController::update()
{
  if (!_timerRunning)
  {
    tick();
  }
}

bool AlarmController::isToneReady() const
{
  return _toneReady;
}

void AlarmController::setServiceAlarm(bool active)
{
  if (active && !_serviceAlarm)
  {
    _silenced = false;
  }

  _serviceAlarm = active;
}

void AlarmController::setNetworkAlarm(bool active)
{
  if (active && !_networkAlarm)
  {
    _silenced = false;
  }

  _networkAlarm = active;
}

void AlarmController::notifyNewFault()
{
  _silenced = false;
}

bool AlarmController::isAlarmActive() const
{
  return _serviceAlarm || _networkAlarm;
}

bool AlarmController::isServiceAlarmActive() const
{
  return _serviceAlarm;
}

bool AlarmController::isNetworkAlarmActive() const
{
  return _networkAlarm;
}

void AlarmController::silence()
{
  _silenced = true;
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
  // Publish the start time before the flag the interrupt checks first.
  _testStartedAtMs = millis();
  _testActive = true;
}

bool AlarmController::isTestActive() const
{
  return _testActive;
}

void AlarmController::setHardwareFaultPattern()
{
  _hardwareFault = true;
}

void AlarmController::timerCallback(timer_callback_args_t *args)
{
  if (args != nullptr && args->p_context != nullptr)
  {
    static_cast<AlarmController *>(const_cast<void *>(args->p_context))->tick();
  }
}

void AlarmController::tick()
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
  }

  if (_silenced)
  {
    writeOutput(false);
    return;
  }

  if (_serviceAlarm)
  {
    // Two short beeps, then a pause.
    const unsigned long position = now % SERVICE_ALARM_PERIOD_MS;
    writeOutput(position < 500UL || (position >= 1000UL && position < 1500UL));
    return;
  }

  if (_networkAlarm)
  {
    // One long beep, then a long pause.
    const unsigned long position = now % NETWORK_ALARM_PERIOD_MS;
    writeOutput(position < 1500UL);
    return;
  }

  writeOutput(false);
}

void AlarmController::writeOutput(bool enabled)
{
  if (!_toneReady || enabled == _outputOn)
  {
    return;
  }

  // The new duty takes effect at the end of the current PWM period.
  _pwm.pulseWidth_raw(static_cast<int>(enabled ? _toneDutyCounts : 0U));
  _outputOn = enabled;
}
