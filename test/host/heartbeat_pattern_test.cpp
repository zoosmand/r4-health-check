/**
  ******************************************************************************
  * @file           : heartbeat_pattern_test.cpp
  * @brief          : Host tests for HeartbeatPattern (test/host/run.sh).
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Host computer (C++17 unit tests)
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

#include "../../src/UNO_R4_Health_Checker/HeartbeatPattern.h"

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

/**
  * @brief On and off edges of the "lub" and "dub" pulses within one period.
  */
static void testPulseWindows()
{
  // "lub": 0-100 ms.
  EXPECT(isHeartbeatLedOn(0, 1000));
  EXPECT(isHeartbeatLedOn(99, 1000));
  EXPECT(!isHeartbeatLedOn(100, 1000));

  // Pause between the pulses.
  EXPECT(!isHeartbeatLedOn(249, 1000));

  // "dub": 250-350 ms.
  EXPECT(isHeartbeatLedOn(250, 1000));
  EXPECT(isHeartbeatLedOn(349, 1000));
  EXPECT(!isHeartbeatLedOn(350, 1000));

  // Long pause until the next beat.
  EXPECT(!isHeartbeatLedOn(999, 1000));
}

/**
  * @brief The pattern repeats every period, for several period lengths.
  */
static void testRepeatsEveryPeriod()
{
  EXPECT(isHeartbeatLedOn(1000, 1000));
  EXPECT(isHeartbeatLedOn(5250, 1000));
  EXPECT(!isHeartbeatLedOn(5500, 1000));

  EXPECT(isHeartbeatLedOn(1500, 1500));
  EXPECT(!isHeartbeatLedOn(1400, 1500));
}

/**
  * @brief Periods below HEARTBEAT_MINIMUM_PERIOD_MS keep the LED dark.
  */
static void testMinimumPeriod()
{
  EXPECT(!isHeartbeatLedOn(0, 0));
  EXPECT(!isHeartbeatLedOn(0, HEARTBEAT_MINIMUM_PERIOD_MS - 1));
  EXPECT(isHeartbeatLedOn(0, HEARTBEAT_MINIMUM_PERIOD_MS));
  EXPECT(!isHeartbeatLedOn(HEARTBEAT_MINIMUM_PERIOD_MS - 1,
                           HEARTBEAT_MINIMUM_PERIOD_MS));
}

/**
  * @brief Elapsed times near ULONG_MAX give the same result as their phase.
  */
static void testLargeElapsedTime()
{
  // No overflow or undefined behavior near the top of the range.
  const unsigned long top = ULONG_MAX;
  const bool expected = isHeartbeatLedOn(top % 1000UL, 1000);
  EXPECT(isHeartbeatLedOn(top, 1000) == expected);
}

/**
  * @brief Run every test and print the totals.
  * @retval (int) 0 when every expectation held, 1 otherwise.
  */
int main()
{
  testPulseWindows();
  testRepeatsEveryPeriod();
  testMinimumPeriod();
  testLargeElapsedTime();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
