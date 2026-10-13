/**
  ******************************************************************************
  * @file           : ResourceAlert.h
  * @brief          : Buzzer alerts that beep the affected resource numbers.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 13.10.2026
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

#ifndef RESOURCE_ALERT_H
#define RESOURCE_ALERT_H

/*
  Buzzer alerts that name the affected resources.

  An alert is a prefix (the SOS signal or the certificate warning melody),
  followed, for each affected resource in ascending order, by a pause and as
  many short beeps as the resource number:

    prefix | pause | n1 beeps | pause | n2 beeps | ...

  Affected resources are passed as a bit mask: bit (n - 1) stands for
  resource number n. A mask fits in one byte, so it can be shared with an
  interrupt without locking.

  Nothing in this module depends on Arduino headers, so it can be compiled
  and tested on the host.
*/

#include <stddef.h>
#include <stdint.h>

#include "BuzzerMelody.h"

// Resource numbers run from 1 to this value.
constexpr uint8_t MAX_RESOURCE_NUMBER = 5;

/**
  * @brief Mask bit of one resource number.
  * @param number (uint8_t) Resource number, 1 to MAX_RESOURCE_NUMBER.
  * @retval (uint8_t) The bit, or 0 when number is out of range.
  */
constexpr uint8_t getResourceBit(uint8_t number)
{
  return number >= 1 && number <= MAX_RESOURCE_NUMBER
           ? static_cast<uint8_t>(1U << (number - 1U))
           : 0;
}

/**
  * @brief Sound and timing of one alert.
  * @param prefix (const MelodyNote*) Signal played first; may be nullptr
  *        when prefixCount is 0. Must outlive every user of the pattern.
  * @param prefixCount (size_t) Number of prefix notes.
  * @param beepHz (uint16_t) Frequency of the counted beeps.
  * @param beepMs (uint16_t) Length of one counted beep.
  * @param beepGapMs (uint16_t) Silence between two beeps of one resource.
  * @param pauseMs (uint16_t) Silence before each resource's beeps.
  */
struct ResourceAlertPattern
{
  const MelodyNote *prefix;
  size_t prefixCount;

  uint16_t beepHz;
  uint16_t beepMs;
  uint16_t beepGapMs;
  uint16_t pauseMs;
};

/**
  * @brief Total length of an alert.
  * @param pattern (const ResourceAlertPattern&) Alert sound and timing.
  * @param resourceMask (uint8_t) Resources to count; bits above
  *        MAX_RESOURCE_NUMBER are ignored.
  * @retval (unsigned long) Length in milliseconds.
  */
unsigned long getResourceAlertDurationMs(
  const ResourceAlertPattern &pattern,
  uint8_t resourceMask
);

/**
  * @brief Find what sounds at a position within an alert. Safe to call from
  *        an interrupt: it does not allocate and runs in bounded time.
  * @param pattern (const ResourceAlertPattern&) Alert sound and timing.
  * @param resourceMask (uint8_t) Resources to count.
  * @param positionMs (unsigned long) Time since the alert started.
  * @param frequencyHz (uint16_t&) Receives the frequency, or 0 while silent.
  * @retval (bool) False once positionMs is past the end of the alert.
  */
bool findResourceAlertFrequency(
  const ResourceAlertPattern &pattern,
  uint8_t resourceMask,
  unsigned long positionMs,
  uint16_t &frequencyHz
);

/*
  Repeats an alert while any resource is affected.

  The alert plays as soon as a resource becomes affected, then once per
  interval. The affected set is captured when an alert starts, so a change
  never cuts into the counting. A resource that becomes affected while an
  alert plays, or between two alerts, is announced right after the current
  alert ends instead of waiting for the next interval.

  Not thread-safe: use one instance from one context (the buzzer tick).
*/
class ResourceAlertSchedule
{
public:
  /**
    * @param pattern (const ResourceAlertPattern&) Alert to play; must
    *        outlive this object.
    * @param intervalMs (unsigned long) Time between the starts of two
    *        alerts. An alert longer than this finishes before the next
    *        starts.
    */
  ResourceAlertSchedule(const ResourceAlertPattern &pattern, unsigned long intervalMs);

  /**
    * @brief Advance the schedule. Call on every buzzer tick, also while
    *        another pattern has priority, so the schedule keeps its cadence.
    * @param affectedMask (uint8_t) Resources affected now; 0 stops the
    *        schedule.
    * @param nowMs (unsigned long) Current millis() value.
    * @param frequencyHz (uint16_t&) Receives the frequency, or 0 while
    *        silent.
    * @retval (bool) True while an alert is playing, including its pauses.
    */
  bool update(uint8_t affectedMask, unsigned long nowMs, uint16_t &frequencyHz);

private:
  const ResourceAlertPattern &_pattern;
  unsigned long _intervalMs;

  bool _scheduled;
  unsigned long _startedAtMs;
  uint8_t _announcedMask;
};

#endif
