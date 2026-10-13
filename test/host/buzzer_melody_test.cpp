/**
  ******************************************************************************
  * @file           : buzzer_melody_test.cpp
  * @brief          : Host tests for BuzzerMelody (test/host/run.sh).
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Host computer (C++17 unit tests)
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

#include "../../src/UNO_R4_Health_Checker/BuzzerMelody.h"

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

static const MelodyNote MELODY[] = {
  {2000, 100},
  {0, 50},
  {3000, 200}
};

static const size_t MELODY_COUNT = sizeof(MELODY) / sizeof(MELODY[0]);

/**
  * @brief Total melody length, including an empty melody.
  */
static void testDuration()
{
  EXPECT(getMelodyDurationMs(MELODY, MELODY_COUNT) == 350UL);
  EXPECT(getMelodyDurationMs(nullptr, 0) == 0UL);
}

/**
  * @brief Note and rest selection at the first and last millisecond of each
  *        note.
  */
static void testNoteBoundaries()
{
  uint16_t frequencyHz = 1;

  EXPECT(findMelodyFrequency(MELODY, MELODY_COUNT, 0, frequencyHz) && frequencyHz == 2000);
  EXPECT(findMelodyFrequency(MELODY, MELODY_COUNT, 99, frequencyHz) && frequencyHz == 2000);

  // The rest.
  EXPECT(findMelodyFrequency(MELODY, MELODY_COUNT, 100, frequencyHz) && frequencyHz == 0);
  EXPECT(findMelodyFrequency(MELODY, MELODY_COUNT, 149, frequencyHz) && frequencyHz == 0);

  EXPECT(findMelodyFrequency(MELODY, MELODY_COUNT, 150, frequencyHz) && frequencyHz == 3000);
  EXPECT(findMelodyFrequency(MELODY, MELODY_COUNT, 349, frequencyHz) && frequencyHz == 3000);
}

/**
  * @brief Positions past the end report silence and the end of the melody.
  */
static void testPastTheEnd()
{
  uint16_t frequencyHz = 1;

  EXPECT(!findMelodyFrequency(MELODY, MELODY_COUNT, 350, frequencyHz) && frequencyHz == 0);
  EXPECT(!findMelodyFrequency(MELODY, MELODY_COUNT, 3600000UL, frequencyHz));
  EXPECT(!findMelodyFrequency(nullptr, 0, 0, frequencyHz));
}

/**
  * @brief Run every test and print the totals.
  * @retval (int) 0 when every expectation held, 1 otherwise.
  */
int main()
{
  testDuration();
  testNoteBoundaries();
  testPastTheEnd();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
