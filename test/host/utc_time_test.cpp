// Host-side unit tests for src/UNO_R4_Health_Checker/UtcTime.cpp. Run with test/host/run.sh.

#include "../../src/UNO_R4_Health_Checker/UtcTime.h"

#include <cstdio>
#include <cstring>

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

static void testMakeUnixSeconds()
{
  uint32_t value = 1;

  EXPECT(makeUnixSeconds(1970, 1, 1, 0, 0, 0, value) && value == 0);
  EXPECT(makeUnixSeconds(2000, 3, 1, 0, 0, 0, value) && value == 951868800UL);
  EXPECT(makeUnixSeconds(2024, 2, 29, 12, 0, 0, value) && value == 1709208000UL);
  EXPECT(makeUnixSeconds(2026, 12, 20, 23, 59, 59, value) && value == 1797811199UL);
  EXPECT(makeUnixSeconds(2105, 12, 31, 23, 59, 59, value) && value == 4291747199UL);

  // Out of range fields.
  EXPECT(!makeUnixSeconds(1969, 12, 31, 23, 59, 59, value));
  EXPECT(!makeUnixSeconds(2106, 1, 1, 0, 0, 0, value));
  EXPECT(!makeUnixSeconds(2023, 2, 29, 0, 0, 0, value));
  EXPECT(!makeUnixSeconds(2100, 2, 29, 0, 0, 0, value));
  EXPECT(!makeUnixSeconds(2026, 4, 31, 0, 0, 0, value));
  EXPECT(!makeUnixSeconds(2026, 0, 1, 0, 0, 0, value));
  EXPECT(!makeUnixSeconds(2026, 13, 1, 0, 0, 0, value));
  EXPECT(!makeUnixSeconds(2026, 1, 1, 24, 0, 0, value));
  EXPECT(!makeUnixSeconds(2026, 1, 1, 0, 60, 0, value));
  EXPECT(!makeUnixSeconds(2026, 1, 1, 0, 0, 61, value));
}

static void testParseHttpDate()
{
  uint32_t value = 0;

  EXPECT(parseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT", value) && value == 784111777UL);
  EXPECT(parseHttpDate("Sun, 20 Dec 2026 23:59:59 GMT", value) && value == 1797811199UL);

  // The day name is not checked against the date.
  EXPECT(parseHttpDate("Mon, 20 Dec 2026 23:59:59 GMT", value) && value == 1797811199UL);

  EXPECT(!parseHttpDate("", value));
  EXPECT(!parseHttpDate("Sunday, 06-Nov-94 08:49:37 GMT", value));
  EXPECT(!parseHttpDate("Sun Nov  6 08:49:37 1994", value));
  EXPECT(!parseHttpDate("Sun, 06 Nov 1994 08:49:37 UTC", value));
  EXPECT(!parseHttpDate("Sun, 06 Nox 1994 08:49:37 GMT", value));
  EXPECT(!parseHttpDate("Sun, 31 Nov 1994 08:49:37 GMT", value));
  EXPECT(!parseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT ", value));
  EXPECT(!parseHttpDate("Sun, 0x Nov 1994 08:49:37 GMT", value));
}

static void testParseAsn1Time()
{
  uint32_t value = 0;

  EXPECT(parseAsn1Time("261220235959Z", 13, value) && value == 1797811199UL);
  EXPECT(parseAsn1Time("491231235959Z", 13, value) && value == 2524607999UL);
  EXPECT(parseAsn1Time("20261220235959Z", 15, value) && value == 1797811199UL);
  EXPECT(parseAsn1Time("20510531135715Z", 15, value) && value == 2569154235UL);

  // UTCTime 50..99 is 1950..1999, before the Unix epoch for 50..69.
  EXPECT(!parseAsn1Time("500101000000Z", 13, value));
  EXPECT(parseAsn1Time("700101000000Z", 13, value) && value == 0);

  EXPECT(!parseAsn1Time("261220235959", 12, value));
  EXPECT(!parseAsn1Time("2612202359590", 13, value));
  EXPECT(!parseAsn1Time("26122023595Z", 12, value));
  EXPECT(!parseAsn1Time("261320235959Z", 13, value));
  EXPECT(!parseAsn1Time("2026122023595Z", 14, value));
}

static void testFormatIsoUtc()
{
  char text[24];

  EXPECT(formatIsoUtc(0, text, sizeof(text)) &&
         std::strcmp(text, "1970-01-01T00:00:00Z") == 0);
  EXPECT(formatIsoUtc(784111777UL, text, sizeof(text)) &&
         std::strcmp(text, "1994-11-06T08:49:37Z") == 0);
  EXPECT(formatIsoUtc(1709208000UL, text, sizeof(text)) &&
         std::strcmp(text, "2024-02-29T12:00:00Z") == 0);
  EXPECT(formatIsoUtc(4294967295UL, text, sizeof(text)) &&
         std::strcmp(text, "2106-02-07T06:28:15Z") == 0);

  EXPECT(formatIsoUtc(0, text, 21) && std::strlen(text) == 20);
  EXPECT(!formatIsoUtc(0, text, 20) && text[0] == '\0');
}

static void testDaysUntil()
{
  const uint32_t now = 1791504000UL;

  EXPECT(daysUntil(now, now) == 0);
  EXPECT(daysUntil(now + 86399UL, now) == 0);
  EXPECT(daysUntil(now + 86400UL, now) == 1);
  EXPECT(daysUntil(now + 10UL * 86400UL, now) == 10);
  EXPECT(daysUntil(now - 1UL, now) == -1);
  EXPECT(daysUntil(now - 86400UL, now) == -1);
  EXPECT(daysUntil(now - 86401UL, now) == -2);
  EXPECT(daysUntil(4294967295UL, 0) == 49710);
  EXPECT(daysUntil(0, 4294967295UL) == -49711);
}

int main()
{
  testMakeUnixSeconds();
  testParseHttpDate();
  testParseAsn1Time();
  testFormatIsoUtc();
  testDaysUntil();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
