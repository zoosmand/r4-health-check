#include "AlarmController.h"
#include "AppConfig.h"

namespace
{
// GPT channel 7 has a 16-bit counter.
constexpr uint32_t MAX_PERIOD_COUNTS = 0xFFFFUL;
}  // namespace

AlarmController::AlarmController(uint8_t pin, uint16_t toneHz)
  : _pin(pin),
    _toneHz(toneHz),
    _pwm(pin),
    _toneReady(false),
    _basePeriodCounts(0),
    _periodCounts(0),
    _periodHz(0),
    _outputHz(0),
    _serviceAlert(SERVICE_ALERT_PATTERN, SERVICE_ALERT_INTERVAL_MS),
    _certificateAlert(CERTIFICATE_ALERT_PATTERN, CERTIFICATE_WARNING_INTERVAL_MS),
    _timerRunning(false),
    _failingResources(0),
    _networkAlarm(false),
    _silenced(false),
    _testActive(false),
    _hardwareFault(false),
    _expiringResources(0),
    _alertTestActive(false),
    _alertTestCertificate(false),
    _alertTestMask(0),
    _testStartedAtMs(0),
    _alertTestStartedAtMs(0)
{
}

bool AlarmController::begin()
{
  // Hold the pin low until the PWM takes it over.
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);

  // Start at 50% so the period is known, then silence at once.
  _toneReady = _pwm.begin(static_cast<float>(_toneHz), 50.0f);

  if (_toneReady)
  {
    _basePeriodCounts = _pwm.get_timer()->get_period_raw();
    _periodCounts = _basePeriodCounts;
    _periodHz = _toneHz;
    _outputHz = _toneHz;
    writeTone(0);
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

void AlarmController::setServiceAlarm(uint8_t failingResources)
{
  if (failingResources != 0 && _failingResources == 0)
  {
    _silenced = false;
  }

  _failingResources = failingResources;
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

void AlarmController::setCertificateWarning(uint8_t expiringResources)
{
  if (expiringResources != 0 && _expiringResources == 0)
  {
    _silenced = false;
  }

  _expiringResources = expiringResources;
}

bool AlarmController::isCertificateWarningActive() const
{
  return _expiringResources != 0;
}

bool AlarmController::isAlarmActive() const
{
  return _failingResources != 0 || _networkAlarm;
}

bool AlarmController::isServiceAlarmActive() const
{
  return _failingResources != 0;
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

void AlarmController::startAlertTest(AlertTest alert, uint8_t resourceMask)
{
  // The interrupt reads the parameters only while the flag is set, and it
  // cannot be interrupted by this code. Clearing the flag first means it
  // never sees a mix of old and new parameters.
  _alertTestActive = false;
  _alertTestCertificate = alert == AlertTest::CERTIFICATE;
  _alertTestMask = resourceMask;
  _alertTestStartedAtMs = millis();
  _alertTestActive = true;
}

bool AlarmController::isAlertTestActive() const
{
  return _alertTestActive;
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

  // Advance the alert schedules first, so they keep their cadence while a
  // higher-priority pattern sounds.
  uint16_t serviceHz = 0;
  _serviceAlert.update(_failingResources, now, serviceHz);

  uint16_t certificateHz = 0;
  const bool certificatePlaying =
    _certificateAlert.update(_expiringResources, now, certificateHz);

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

  if (_alertTestActive)
  {
    const ResourceAlertPattern &pattern =
      _alertTestCertificate ? CERTIFICATE_ALERT_PATTERN : SERVICE_ALERT_PATTERN;

    uint16_t testHz = 0;

    if (findResourceAlertFrequency(
          pattern,
          _alertTestMask,
          now - _alertTestStartedAtMs,
          testHz
        ))
    {
      writeTone(testHz);
      return;
    }

    _alertTestActive = false;
  }

  if (_silenced)
  {
    writeOutput(false);
    return;
  }

  if (_failingResources != 0)
  {
    // SOS and the failing resource numbers, then silence until the next
    // slot. The silence also keeps the lower-priority patterns quiet.
    writeTone(serviceHz);
    return;
  }

  if (_networkAlarm)
  {
    // One long beep, then a long pause.
    const unsigned long position = now % NETWORK_ALARM_PERIOD_MS;
    writeOutput(position < 1500UL);
    return;
  }

  if (certificatePlaying)
  {
    writeTone(certificateHz);
    return;
  }

  writeOutput(false);
}

void AlarmController::writeOutput(bool enabled)
{
  writeTone(enabled ? _toneHz : 0);
}

void AlarmController::writeTone(uint16_t frequencyHz)
{
  if (!_toneReady || frequencyHz == _outputHz)
  {
    return;
  }

  if (frequencyHz != 0 && frequencyHz != _periodHz)
  {
    // The period scales inversely with the frequency at a fixed prescaler.
    const uint32_t periodCounts =
      _basePeriodCounts * _toneHz / frequencyHz;

    if (periodCounts < 2 || periodCounts > MAX_PERIOD_COUNTS)
    {
      // Out of range for the counter: play as a rest.
      frequencyHz = 0;
    }
    else
    {
      // The FSP driver compares a new duty with the active period (a duty
      // at or above it means 100%), while a new period only takes effect
      // at the next counter overflow. So silence first, load the period,
      // and start the note on the next tick, by which time the period is
      // active.
      if (_outputHz != 0)
      {
        _pwm.pulseWidth_raw(0);
        _outputHz = 0;
      }

      _pwm.period_raw(static_cast<int>(periodCounts));
      _periodCounts = periodCounts;
      _periodHz = frequencyHz;
      return;
    }
  }

  // The new duty takes effect at the end of the current PWM period.
  _pwm.pulseWidth_raw(
    static_cast<int>(frequencyHz == 0 ? 0U : _periodCounts / 2U)
  );
  _outputHz = frequencyHz;
}
