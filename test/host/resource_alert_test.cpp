/**
  ******************************************************************************
  * @file           : resource_alert_test.cpp
  * @brief          : Host tests for ResourceAlert (test/host/run.sh).
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Host computer (C++17 unit tests)
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

#include "../../src/UNO_R4_Health_Checker/ResourceAlert.h"

#include <climits>
#include <cstdio>

static int failures = 0;
static int checks = 0;

#define EXPECT(condition)                                                    \
  do                                                                         \
  {                                                                          \
    checks++;                                                                \
    if (!(condition))                                                        \
    {                                                                        \
      failures++;                                                            \
      std::printf("%s:%d: expectation failed: %s\n", __FILE__, __LINE__,    \
                  #condition);                                               \
    }                                                                        \
  } while (0)

// Prefix: 100 ms tone, 50 ms rest. Beeps: 20 ms on, 30 ms off. Pause: 100 ms.
static const MelodyNote PREFIX[] = {
  {1000, 100},
  {0, 50}
};

static const ResourceAlertPattern PATTERN = {
  PREFIX,
  sizeof(PREFIX) / sizeof(PREFIX[0]),
  2000,
  20,
  30,
  100
};

constexpr uint8_t RESOURCE_2 = getResourceBit(2);
constexpr uint8_t RESOURCE_3 = getResourceBit(3);

/**
  * @brief Frequency of PATTERN at a position.
  * @param mask (uint8_t) Resources to count.
  * @param positionMs (unsigned long) Time since the alert started.
  * @retval (uint16_t) Frequency, or 0 while silent.
  */
static uint16_t frequencyAt(uint8_t mask, unsigned long positionMs)
{
  uint16_t frequencyHz = 0xFFFF;
  findResourceAlertFrequency(PATTERN, mask, positionMs, frequencyHz);
  return frequencyHz;
}

/**
  * @brief Count the beeps that start in [fromMs, toMs).
  */
static int countBeeps(uint8_t mask, unsigned long fromMs, unsigned long toMs)
{
  int beeps = 0;
  bool wasOn = false;

  for (unsigned long t = fromMs; t < toMs; t++)
  {
    const bool on = frequencyAt(mask, t) == PATTERN.beepHz;

    if (on && !wasOn)
    {
      beeps++;
    }

    wasOn = on;
  }

  return beeps;
}

/**
  * @brief Mask bits of valid and out-of-range resource numbers.
  */
static void testResourceBit()
{
  EXPECT(getResourceBit(0) == 0);
  EXPECT(getResourceBit(1) == 0x01);
  EXPECT(getResourceBit(5) == 0x10);
  EXPECT(getResourceBit(MAX_RESOURCE_NUMBER + 1) == 0);
}

/**
  * @brief Alert length for no, one, and two resources, ignoring invalid bits.
  */
static void testDuration()
{
  // Prefix only.
  EXPECT(getResourceAlertDurationMs(PATTERN, 0) == 150);

  // Resource 2: pause 100 + 2 beeps (20 + 30 + 20).
  EXPECT(getResourceAlertDurationMs(PATTERN, RESOURCE_2) == 320);

  // Resources 2 and 3: 320 + pause 100 + 3 beeps (3 * 20 + 2 * 30).
  EXPECT(getResourceAlertDurationMs(PATTERN, RESOURCE_2 | RESOURCE_3) == 540);

  // Bits above MAX_RESOURCE_NUMBER are ignored.
  EXPECT(getResourceAlertDurationMs(PATTERN, 0xE0) == 150);
}

/**
  * @brief Prefix, pauses, and counted beeps at their exact positions.
  */
static void testLayout()
{
  const uint8_t mask = RESOURCE_2 | RESOURCE_3;

  // Prefix.
  EXPECT(frequencyAt(mask, 0) == 1000);
  EXPECT(frequencyAt(mask, 99) == 1000);
  EXPECT(frequencyAt(mask, 100) == 0);

  // Pause before resource 2.
  EXPECT(frequencyAt(mask, 150) == 0);
  EXPECT(frequencyAt(mask, 249) == 0);

  // Resource 2: beeps at 250-269 and 300-319.
  EXPECT(frequencyAt(mask, 250) == 2000);
  EXPECT(frequencyAt(mask, 269) == 2000);
  EXPECT(frequencyAt(mask, 270) == 0);
  EXPECT(frequencyAt(mask, 300) == 2000);
  EXPECT(frequencyAt(mask, 320) == 0);

  // Resource 3 starts after the next pause, at 420.
  EXPECT(frequencyAt(mask, 419) == 0);
  EXPECT(frequencyAt(mask, 420) == 2000);
  EXPECT(frequencyAt(mask, 539) == 2000);

  EXPECT(countBeeps(mask, 150, 420) == 2);
  EXPECT(countBeeps(mask, 420, 540) == 3);

  // Past the end.
  uint16_t frequencyHz = 0xFFFF;
  EXPECT(findResourceAlertFrequency(PATTERN, mask, 539, frequencyHz));
  EXPECT(!findResourceAlertFrequency(PATTERN, mask, 540, frequencyHz));
  EXPECT(frequencyHz == 0);
}

/**
  * @brief Each resource number from 1 to 5 is beeped that many times.
  */
static void testEveryResourceCount()
{
  for (uint8_t number = 1; number <= MAX_RESOURCE_NUMBER; number++)
  {
    const uint8_t mask = getResourceBit(number);
    EXPECT(countBeeps(mask, 150, getResourceAlertDurationMs(PATTERN, mask)) == number);
  }
}

/**
  * @brief An alert plays at once and then once per interval.
  */
static void testScheduleRepeatsEveryInterval()
{
  ResourceAlertSchedule schedule(PATTERN, 1000);
  uint16_t frequencyHz = 0;

  EXPECT(!schedule.update(0, 0, frequencyHz));

  // Plays at once, for 320 ms.
  EXPECT(schedule.update(RESOURCE_2, 10, frequencyHz));
  EXPECT(frequencyHz == 1000);
  EXPECT(schedule.update(RESOURCE_2, 329, frequencyHz));
  EXPECT(!schedule.update(RESOURCE_2, 330, frequencyHz));
  EXPECT(frequencyHz == 0);

  // Silent until the next interval.
  EXPECT(!schedule.update(RESOURCE_2, 1009, frequencyHz));
  EXPECT(schedule.update(RESOURCE_2, 1010, frequencyHz));
  EXPECT(frequencyHz == 1000);
}

/**
  * @brief A resource that fails during an alert is named right after it,
  *        without changing the alert in progress.
  */
static void testScheduleAnnouncesNewResourceAfterCurrentAlert()
{
  ResourceAlertSchedule schedule(PATTERN, 1000);
  uint16_t frequencyHz = 0;

  EXPECT(schedule.update(RESOURCE_2, 0, frequencyHz));

  // Resource 3 fails while the alert plays: the counting is not changed.
  EXPECT(schedule.update(RESOURCE_2 | RESOURCE_3, 250, frequencyHz));
  EXPECT(frequencyHz == 2000);
  EXPECT(schedule.update(RESOURCE_2 | RESOURCE_3, 319, frequencyHz));

  // Right after the end, a new alert names both resources.
  EXPECT(schedule.update(RESOURCE_2 | RESOURCE_3, 320, frequencyHz));
  EXPECT(frequencyHz == 1000);
  EXPECT(schedule.update(RESOURCE_2 | RESOURCE_3, 320 + 539, frequencyHz));
  EXPECT(!schedule.update(RESOURCE_2 | RESOURCE_3, 320 + 540, frequencyHz));
}

/**
  * @brief A resource that recovers and fails again is announced at once.
  */
static void testScheduleReannouncesRecoveredResource()
{
  ResourceAlertSchedule schedule(PATTERN, 1000);
  uint16_t frequencyHz = 0;

  EXPECT(schedule.update(RESOURCE_2 | RESOURCE_3, 0, frequencyHz));
  EXPECT(!schedule.update(RESOURCE_2 | RESOURCE_3, 600, frequencyHz));

  // Resource 2 recovers: no new alert.
  EXPECT(!schedule.update(RESOURCE_3, 700, frequencyHz));

  // Resource 2 fails again before the interval: announced at once.
  EXPECT(schedule.update(RESOURCE_2 | RESOURCE_3, 800, frequencyHz));
  EXPECT(frequencyHz == 1000);
}

/**
  * @brief Clearing every resource stops the schedule; the next one plays at
  *        once.
  */
static void testScheduleRestartsAfterClear()
{
  ResourceAlertSchedule schedule(PATTERN, 1000);
  uint16_t frequencyHz = 0;

  EXPECT(schedule.update(RESOURCE_2, 0, frequencyHz));
  EXPECT(!schedule.update(0, 100, frequencyHz));
  EXPECT(frequencyHz == 0);

  EXPECT(schedule.update(RESOURCE_2, 500, frequencyHz));
  EXPECT(frequencyHz == 1000);
}

/**
  * @brief The schedule keeps its timing across a millis() wrap-around.
  */
static void testScheduleAcrossMillisWrap()
{
  ResourceAlertSchedule schedule(PATTERN, 1000);
  uint16_t frequencyHz = 0;

  const unsigned long start = ULONG_MAX - 100;

  EXPECT(schedule.update(RESOURCE_2, start, frequencyHz));
  EXPECT(schedule.update(RESOURCE_2, start + 319, frequencyHz));
  EXPECT(!schedule.update(RESOURCE_2, start + 320, frequencyHz));
  EXPECT(schedule.update(RESOURCE_2, start + 1000, frequencyHz));
}

/**
  * @brief An interval shorter than the alert does not cut the alert short.
  */
static void testIntervalShorterThanAlert()
{
  // The alert finishes before the next one starts.
  ResourceAlertSchedule schedule(PATTERN, 200);
  uint16_t frequencyHz = 0;

  EXPECT(schedule.update(RESOURCE_2, 0, frequencyHz));
  EXPECT(schedule.update(RESOURCE_2, 250, frequencyHz));
  EXPECT(frequencyHz == 2000);
}

/**
  * @brief Run every test and print the totals.
  * @retval (int) 0 when every expectation held, 1 otherwise.
  */
int main()
{
  testResourceBit();
  testDuration();
  testLayout();
  testEveryResourceCount();
  testScheduleRepeatsEveryInterval();
  testScheduleAnnouncesNewResourceAfterCurrentAlert();
  testScheduleReannouncesRecoveredResource();
  testScheduleRestartsAfterClear();
  testScheduleAcrossMillisWrap();
  testIntervalShorterThanAlert();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
