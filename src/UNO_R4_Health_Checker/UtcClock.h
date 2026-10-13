/**
  ******************************************************************************
  * @file           : UtcClock.h
  * @brief          : Wall-clock time set from HTTP Date headers.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 09.10.2026
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
  /**
    * @brief Create a clock that is not set.
    */
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
