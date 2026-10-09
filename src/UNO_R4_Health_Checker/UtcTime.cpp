#include "UtcTime.h"

#include <stdio.h>
#include <string.h>

namespace
{
bool isLeapYear(int year)
{
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month)
{
  static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeapYear(year) ? 29 : DAYS[month - 1];
}

/**
  * @brief Days since 1970-01-01 for a valid civil date (H. Hinnant's
  *        days_from_civil algorithm, restricted to years from 1970).
  */
long daysFromCivil(int year, int month, int day)
{
  year -= month <= 2 ? 1 : 0;
  const long era = year / 400;
  const long yearOfEra = year - era * 400;
  const long dayOfYear = (153L * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097L + dayOfEra - 719468L;
}

/**
  * @brief Inverse of daysFromCivil for days since 1970-01-01.
  */
void civilFromDays(long days, int &year, int &month, int &day)
{
  days += 719468L;
  const long era = days / 146097L;
  const long dayOfEra = days - era * 146097L;
  const long yearOfEra =
    (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const long dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const long monthIndex = (5 * dayOfYear + 2) / 153;

  day = static_cast<int>(dayOfYear - (153 * monthIndex + 2) / 5 + 1);
  month = static_cast<int>(monthIndex < 10 ? monthIndex + 3 : monthIndex - 9);
  year = static_cast<int>(yearOfEra + era * 400 + (month <= 2 ? 1 : 0));
}

/**
  * @brief Read exactly count decimal digits.
  * @retval (bool) False when a character is not a digit.
  */
bool readDigits(const char *text, size_t count, int &value)
{
  value = 0;

  for (size_t i = 0; i < count; i++)
  {
    if (text[i] < '0' || text[i] > '9')
    {
      return false;
    }

    value = value * 10 + (text[i] - '0');
  }

  return true;
}
}  // namespace

bool makeUnixSeconds(
  int year,
  int month,
  int day,
  int hour,
  int minute,
  int second,
  uint32_t &unixSeconds
)
{
  if (year < 1970 || year > 2105 ||
      month < 1 || month > 12 ||
      day < 1 || day > daysInMonth(year, month) ||
      hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 ||
      second < 0 || second > 60)
  {
    return false;
  }

  const unsigned long long total =
    static_cast<unsigned long long>(daysFromCivil(year, month, day)) * SECONDS_PER_DAY +
    static_cast<unsigned long long>(hour) * 3600ULL +
    static_cast<unsigned long long>(minute) * 60ULL +
    static_cast<unsigned long long>(second);

  if (total > 0xFFFFFFFFULL)
  {
    return false;
  }

  unixSeconds = static_cast<uint32_t>(total);
  return true;
}

bool parseHttpDate(const char *value, uint32_t &unixSeconds)
{
  static const char MONTHS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

  // "Sun, 06 Nov 1994 08:49:37 GMT" is exactly 29 characters.
  if (strlen(value) != 29 ||
      value[3] != ',' || value[4] != ' ' || value[7] != ' ' ||
      value[11] != ' ' || value[16] != ' ' || value[19] != ':' ||
      value[22] != ':' || strcmp(value + 25, " GMT") != 0)
  {
    return false;
  }

  int month = 0;

  for (int i = 0; i < 12; i++)
  {
    if (strncmp(value + 8, MONTHS + i * 3, 3) == 0)
    {
      month = i + 1;
      break;
    }
  }

  int day = 0;
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  return month != 0 &&
         readDigits(value + 5, 2, day) &&
         readDigits(value + 12, 4, year) &&
         readDigits(value + 17, 2, hour) &&
         readDigits(value + 20, 2, minute) &&
         readDigits(value + 23, 2, second) &&
         makeUnixSeconds(year, month, day, hour, minute, second, unixSeconds);
}

bool parseAsn1Time(const char *text, size_t length, uint32_t &unixSeconds)
{
  if ((length != 13 && length != 15) || text[length - 1] != 'Z')
  {
    return false;
  }

  int year = 0;
  size_t offset = 0;

  if (length == 13)
  {
    // RFC 5280: UTCTime years 50..99 are 19xx, 00..49 are 20xx.
    if (!readDigits(text, 2, year))
    {
      return false;
    }

    year += year < 50 ? 2000 : 1900;
    offset = 2;
  }
  else
  {
    if (!readDigits(text, 4, year))
    {
      return false;
    }

    offset = 4;
  }

  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  return readDigits(text + offset, 2, month) &&
         readDigits(text + offset + 2, 2, day) &&
         readDigits(text + offset + 4, 2, hour) &&
         readDigits(text + offset + 6, 2, minute) &&
         readDigits(text + offset + 8, 2, second) &&
         makeUnixSeconds(year, month, day, hour, minute, second, unixSeconds);
}

bool formatIsoUtc(uint32_t unixSeconds, char *out, size_t capacity)
{
  if (capacity < 21)
  {
    if (capacity > 0)
    {
      out[0] = '\0';
    }

    return false;
  }

  const long days = static_cast<long>(unixSeconds / SECONDS_PER_DAY);
  const uint32_t secondOfDay = unixSeconds % SECONDS_PER_DAY;

  int year = 0;
  int month = 0;
  int day = 0;
  civilFromDays(days, year, month, day);

  snprintf(
    out,
    capacity,
    "%04d-%02d-%02dT%02u:%02u:%02uZ",
    year,
    month,
    day,
    static_cast<unsigned>(secondOfDay / 3600U),
    static_cast<unsigned>(secondOfDay / 60U % 60U),
    static_cast<unsigned>(secondOfDay % 60U)
  );

  return true;
}

long daysUntil(uint32_t untilUnixSeconds, uint32_t nowUnixSeconds)
{
  const long long difference =
    static_cast<long long>(untilUnixSeconds) - static_cast<long long>(nowUnixSeconds);

  // Round toward negative infinity, so an expired time is at least -1.
  if (difference >= 0)
  {
    return static_cast<long>(difference / SECONDS_PER_DAY);
  }

  return static_cast<long>(-((-difference + SECONDS_PER_DAY - 1) / SECONDS_PER_DAY));
}
