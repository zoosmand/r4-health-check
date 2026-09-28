// Host-side unit tests for src/UNO_R4_Health_Checker/HeartbeatPattern.cpp. Run with test/host/run.sh.

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

static void testRepeatsEveryPeriod()
{
  EXPECT(isHeartbeatLedOn(1000, 1000));
  EXPECT(isHeartbeatLedOn(5250, 1000));
  EXPECT(!isHeartbeatLedOn(5500, 1000));

  EXPECT(isHeartbeatLedOn(1500, 1500));
  EXPECT(!isHeartbeatLedOn(1400, 1500));
}

static void testMinimumPeriod()
{
  EXPECT(!isHeartbeatLedOn(0, 0));
  EXPECT(!isHeartbeatLedOn(0, HEARTBEAT_MINIMUM_PERIOD_MS - 1));
  EXPECT(isHeartbeatLedOn(0, HEARTBEAT_MINIMUM_PERIOD_MS));
  EXPECT(!isHeartbeatLedOn(HEARTBEAT_MINIMUM_PERIOD_MS - 1,
                           HEARTBEAT_MINIMUM_PERIOD_MS));
}

static void testLargeElapsedTime()
{
  // No overflow or undefined behavior near the top of the range.
  const unsigned long top = ULONG_MAX;
  const bool expected = isHeartbeatLedOn(top % 1000UL, 1000);
  EXPECT(isHeartbeatLedOn(top, 1000) == expected);
}

int main()
{
  testPulseWindows();
  testRepeatsEveryPeriod();
  testMinimumPeriod();
  testLargeElapsedTime();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
