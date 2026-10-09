#ifndef ALARM_CONTROLLER_H
#define ALARM_CONTROLLER_H

#include <Arduino.h>
#include <FspTimer.h>
#include <pwm.h>

/*
  Passive buzzer driver.

  The tone is a hardware PWM square wave on the buzzer pin. The pattern
  switches its duty between 50% (sounding) and 0% (silent); the certificate
  warning melody also changes the PWM period for each note.

  The output pattern is generated from a periodic hardware-timer interrupt,
  so the buzzer keeps its pattern while the main loop is blocked in a DNS
  lookup, TLS handshake, or other long Wi-Fi module call.

  All setters are called from thread context. The interrupt only reads the
  alarm flags and may clear the test flag; every shared flag is a single
  volatile word, so no locking is needed.

  Pattern priority: hardware fault > buzzer test > silenced >
  service alarm > network alarm > certificate warning melody.

  The melody keeps its hourly schedule while a higher-priority pattern
  sounds or the buzzer is silenced; a slot that falls in that time is
  skipped, not postponed.
*/
class AlarmController
{
public:
  /**
    * @param pin (uint8_t) PWM-capable buzzer pin.
    * @param toneHz (uint16_t) Frequency of the alarm patterns.
    */
  AlarmController(uint8_t pin, uint16_t toneHz);

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
    * @brief Set whether at least one server certificate is about to expire.
    *        While true, CERTIFICATE_WARNING_MELODY plays at once and then
    *        every CERTIFICATE_WARNING_INTERVAL_MS. A false-to-true transition
    *        cancels a manual silence.
    * @param active (bool) Aggregate certificate warning state.
    */
  void setCertificateWarning(bool active);
  bool isCertificateWarningActive() const;

  /**
    * @brief Report whether any alarm source is active. The certificate
    *        warning is not an alarm and is not included.
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

  /**
    * @brief Advance the hourly melody schedule.
    * @param now (unsigned long) Current millis() value.
    * @param frequencyHz (uint16_t&) Receives the note or 0 during a rest.
    * @retval (bool) True while the melody is playing.
    */
  bool updateMelody(unsigned long now, uint16_t &frequencyHz);

  void writeOutput(bool enabled);

  /**
    * @brief Sound a frequency, or silence the buzzer with 0. A frequency
    *        change silences the buzzer for one tick and loads the new
    *        period, so the duty is never set against the old, shorter
    *        period. Interrupt context only.
    */
  void writeTone(uint16_t frequencyHz);

  uint8_t _pin;
  uint16_t _toneHz;

  PwmOut _pwm;
  bool _toneReady;

  // PWM state, owned by tick(). _basePeriodCounts is the period at _toneHz
  // and scales to any other frequency with the same prescaler.
  uint32_t _basePeriodCounts;
  uint32_t _periodCounts;
  uint16_t _periodHz;
  uint16_t _outputHz;

  // Melody schedule, owned by tick().
  bool _melodyScheduled;
  unsigned long _melodyStartedAtMs;

  FspTimer _timer;
  bool _timerRunning;

  volatile bool _serviceAlarm;
  volatile bool _networkAlarm;
  volatile bool _silenced;
  volatile bool _testActive;
  volatile bool _hardwareFault;
  volatile bool _certificateWarning;
  volatile unsigned long _testStartedAtMs;
};

#endif
