#ifndef UTC_CLOCK_H
#define UTC_CLOCK_H

#include <Arduino.h>

/*
  Wall-clock time derived from millis().

  The firmware has no NTP client and does not use the RTC. Instead, every
  health-check response carries an HTTP Date header, and each one resets the
  clock. Between those, the time advances with millis(). Accuracy is that of
  the server clocks, which is ample for counting days to a certificate's
  expiry.

  millis() wraps after about 49.7 days. The clock stays correct as long as it
  is set at least once in that time, which the health checks do every
  minute.
*/
class UtcClock
{
public:
  UtcClock();

  /**
    * @brief Set the current time. Values before 2026 are rejected as a
    *        misconfigured server clock.
    * @param unixSeconds (uint32_t) Current UTC time in Unix seconds.
    * @retval (bool) True when the value was accepted.
    */
  bool set(uint32_t unixSeconds);

  /**
    * @brief Report whether set() has accepted a time since boot.
    */
  bool isSet() const;

  /**
    * @brief Current UTC time.
    * @retval (uint32_t) Unix seconds, or 0 while the clock is not set.
    */
  uint32_t nowUnixSeconds() const;

private:
  bool _set;
  uint32_t _unixSecondsAtSet;
  unsigned long _setAtMs;
};

#endif
