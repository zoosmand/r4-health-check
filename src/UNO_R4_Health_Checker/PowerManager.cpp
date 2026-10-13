/**
  ******************************************************************************
  * @file           : PowerManager.cpp
  * @brief          : RA4M1 Sleep mode between main loop passes.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 28.09.2026
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

#include "PowerManager.h"

PowerManager::PowerManager(Watchdog &watchdog)
  : _watchdog(watchdog),
    _enabled(false)
{
}

bool PowerManager::begin()
{
  // SBYCR.SSBY selects the mode WFI enters: 0 is Sleep mode, 1 is Software
  // Standby. Nothing in the core sets it; refuse to sleep if something did.
  _enabled = R_SYSTEM->SBYCR_b.SSBY == 0;
  return _enabled;
}

void PowerManager::idle(unsigned long durationMs)
{
  const unsigned long startedAt = millis();

  while (millis() - startedAt < durationMs)
  {
    if (!_enabled || _watchdog.isResetRequested())
    {
      return;
    }

    // The millis() tick wakes the CPU at least once per millisecond, so a
    // reset request or wake-up reason that arrives just before WFI costs at
    // most one tick.
    __DSB();
    __WFI();
  }
}

bool PowerManager::isEnabled() const
{
  return _enabled;
}
