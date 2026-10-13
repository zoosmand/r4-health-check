/**
  ******************************************************************************
  * @file           : UtcClock.cpp
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

#include "UtcClock.h"

namespace
{
// 2026-01-01T00:00:00Z.
constexpr uint32_t MINIMUM_VALID_UNIX_SECONDS = 1767225600UL;
}  // namespace

UtcClock::UtcClock()
  : _set(false),
    _unixSecondsAtSet(0),
    _setAtMs(0)
{
}

bool UtcClock::set(uint32_t unixSeconds)
{
  if (unixSeconds < MINIMUM_VALID_UNIX_SECONDS)
  {
    return false;
  }

  _unixSecondsAtSet = unixSeconds;
  _setAtMs = millis();
  _set = true;
  return true;
}

bool UtcClock::isSet() const
{
  return _set;
}

uint32_t UtcClock::nowUnixSeconds() const
{
  if (!_set)
  {
    return 0;
  }

  return _unixSecondsAtSet + (millis() - _setAtMs) / 1000UL;
}
