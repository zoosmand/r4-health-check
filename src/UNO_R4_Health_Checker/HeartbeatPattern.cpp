/**
  ******************************************************************************
  * @file           : HeartbeatPattern.cpp
  * @brief          : Timing of the heartbeat LED's double pulse.
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

#include "HeartbeatPattern.h"

namespace
{
// Pulse windows within one period: "lub" 0-100 ms, "dub" 250-350 ms.
constexpr unsigned long FIRST_PULSE_END_MS = 100UL;
constexpr unsigned long SECOND_PULSE_START_MS = 250UL;
constexpr unsigned long SECOND_PULSE_END_MS = 350UL;

static_assert(
  SECOND_PULSE_END_MS < HEARTBEAT_MINIMUM_PERIOD_MS,
  "The heartbeat pulses must fit into the minimum period"
);
}  // namespace

bool isHeartbeatLedOn(unsigned long elapsedMs, unsigned long periodMs)
{
  if (periodMs < HEARTBEAT_MINIMUM_PERIOD_MS)
  {
    return false;
  }

  const unsigned long phaseMs = elapsedMs % periodMs;

  return phaseMs < FIRST_PULSE_END_MS ||
         (phaseMs >= SECOND_PULSE_START_MS && phaseMs < SECOND_PULSE_END_MS);
}
