/**
  ******************************************************************************
  * @file           : BuzzerMelody.cpp
  * @brief          : Melody description and lookup for the passive buzzer.
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

#include "BuzzerMelody.h"

unsigned long getMelodyDurationMs(const MelodyNote *notes, size_t count)
{
  unsigned long total = 0;

  for (size_t i = 0; i < count; i++)
  {
    total += notes[i].durationMs;
  }

  return total;
}

bool findMelodyFrequency(
  const MelodyNote *notes,
  size_t count,
  unsigned long positionMs,
  uint16_t &frequencyHz
)
{
  for (size_t i = 0; i < count; i++)
  {
    if (positionMs < notes[i].durationMs)
    {
      frequencyHz = notes[i].frequencyHz;
      return true;
    }

    positionMs -= notes[i].durationMs;
  }

  frequencyHz = 0;
  return false;
}
