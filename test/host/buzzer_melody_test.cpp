// Host-side unit tests for src/UNO_R4_Health_Checker/BuzzerMelody.cpp. Run with test/host/run.sh.

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

static void testDuration()
{
  EXPECT(getMelodyDurationMs(MELODY, MELODY_COUNT) == 350UL);
  EXPECT(getMelodyDurationMs(nullptr, 0) == 0UL);
}

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

static void testPastTheEnd()
{
  uint16_t frequencyHz = 1;

  EXPECT(!findMelodyFrequency(MELODY, MELODY_COUNT, 350, frequencyHz) && frequencyHz == 0);
  EXPECT(!findMelodyFrequency(MELODY, MELODY_COUNT, 3600000UL, frequencyHz));
  EXPECT(!findMelodyFrequency(nullptr, 0, 0, frequencyHz));
}

int main()
{
  testDuration();
  testNoteBoundaries();
  testPastTheEnd();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
