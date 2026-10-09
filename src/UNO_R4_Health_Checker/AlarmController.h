#ifndef ALARM_CONTROLLER_H
#define ALARM_CONTROLLER_H

#include <Arduino.h>
#include <FspTimer.h>
#include <pwm.h>

/*
  Passive buzzer driver.

  The tone is a hardware PWM square wave on the buzzer pin. The pattern only
  switches its duty between 50% (sounding) and 0% (silent).

  The output pattern is generated from a periodic hardware-timer interrupt,
  so the buzzer keeps its pattern while the main loop is blocked in a DNS
  lookup, TLS handshake, or other long Wi-Fi module call.

  All setters are called from thread context. The interrupt only reads the
  alarm flags and may clear the test flag; every shared flag is a single
  volatile word, so no locking is needed.

  Pattern priority: hardware fault > buzzer test > silenced >
  service alarm > network alarm.
*/
class AlarmController
{
public:
  AlarmController(uint8_t pin, float toneHz);

  /**
    * @brief Start the tone PWM (silent) and the pattern timer.
    * @retval (bool) True when a hardware timer was acquired. When false, the
    *         pattern is only advanced by update() calls from the main loop.
    *         The PWM result is reported separately by isToneReady().
    */
  bool begin();

  /**
    * @brief Report whether the tone PWM was started on the buzzer pin.
    * @retval (bool) False when the pin has no PWM channel or the channel is
    *         taken; the buzzer then stays silent.
    */
  bool isToneReady() const;

  /**
    * @brief Advance the pattern from the main loop. Needed only when begin()
    *        could not acquire a timer; harmless otherwise.
    */
  void update();

  /**
    * @brief Set whether at least one monitored service is failing.
    *        A false-to-true transition cancels a manual silence.
    * @param active (bool) Aggregate service alarm state.
    */
  void setServiceAlarm(bool active);

  /**
    * @brief Set whether the device itself has lost Wi-Fi for too long.
    *        A false-to-true transition cancels a manual silence.
    * @param active (bool) Network alarm state.
    */
  void setNetworkAlarm(bool active);

  /**
    * @brief Report a new fault while an alarm may already be active, for
    *        example a second service failing. Cancels a manual silence.
    */
  void notifyNewFault();

  /**
    * @brief Report whether any alarm source is active.
    * @retval (bool) True when the service or network alarm is active.
    */
  bool isAlarmActive() const;
  bool isServiceAlarmActive() const;
  bool isNetworkAlarmActive() const;

  void silence();
  void unsilence();
  bool isSilenced() const;

  /**
    * @brief Sound the buzzer continuously for BUZZER_TEST_DURATION_MS.
    */
  void startTest();
  bool isTestActive() const;

  /**
    * @brief Switch permanently to the fast hardware-fault pattern.
    */
  void setHardwareFaultPattern();

private:
  static void timerCallback(timer_callback_args_t *args);

  /**
    * @brief Compute and write the output for the current time. Runs in the
    *        timer interrupt, or in update() when no timer is available.
    */
  void tick();
  void writeOutput(bool enabled);

  uint8_t _pin;
  float _toneHz;

  PwmOut _pwm;
  bool _toneReady;
  uint32_t _toneDutyCounts;
  bool _outputOn;

  FspTimer _timer;
  bool _timerRunning;

  volatile bool _serviceAlarm;
  volatile bool _networkAlarm;
  volatile bool _silenced;
  volatile bool _testActive;
  volatile bool _hardwareFault;
  volatile unsigned long _testStartedAtMs;
};

#endif
