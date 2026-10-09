#ifndef UTC_TIME_H
#define UTC_TIME_H

/*
  Pure UTC calendar helpers shared by the firmware and the host-side unit
  tests. Times are Unix seconds in an unsigned 32-bit value, which covers
  1970-01-01 to 2106-02-07.

  Nothing in this module depends on Arduino headers.
*/

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t SECONDS_PER_DAY = 86400UL;

/**
  * @brief Convert a UTC calendar time to Unix seconds.
  * @param year (int) Full year, 1970..2105.
  * @param month (int) Month, 1..12.
  * @param day (int) Day of the month, 1..28/29/30/31.
  * @param hour (int) Hour, 0..23.
  * @param minute (int) Minute, 0..59.
  * @param second (int) Second, 0..60 (a leap second counts as 60).
  * @param unixSeconds (uint32_t&) Receives the result on success.
  * @retval (bool) False when a field is out of range or the time does not fit.
  */
bool makeUnixSeconds(
  int year,
  int month,
  int day,
  int hour,
  int minute,
  int second,
  uint32_t &unixSeconds
);

/**
  * @brief Parse an HTTP Date header value in IMF-fixdate form,
  *        for example "Sun, 06 Nov 1994 08:49:37 GMT".
  * @param value (const char*) Non-null header value without the name.
  * @param unixSeconds (uint32_t&) Receives the result on success.
  * @retval (bool) False for any other format or an invalid date.
  */
bool parseHttpDate(const char *value, uint32_t &unixSeconds);

/**
  * @brief Parse the contents of an X.509 time: UTCTime "YYMMDDHHMMSSZ" or
  *        GeneralizedTime "YYYYMMDDHHMMSSZ" (RFC 5280, 4.1.2.5).
  * @param text (const char*) Non-null characters, not NUL-terminated.
  * @param length (size_t) 13 for UTCTime, 15 for GeneralizedTime.
  * @param unixSeconds (uint32_t&) Receives the result on success.
  * @retval (bool) False for any other length, format, or an invalid date.
  */
bool parseAsn1Time(const char *text, size_t length, uint32_t &unixSeconds);

/**
  * @brief Format Unix seconds as "YYYY-MM-DDTHH:MM:SSZ".
  * @param unixSeconds (uint32_t) Time to format.
  * @param out (char*) Non-null buffer of at least 21 bytes; NUL-terminated.
  * @param capacity (size_t) Size of out in bytes.
  * @retval (bool) False when out is too small; out is then an empty string
  *         if capacity is at least 1.
  */
bool formatIsoUtc(uint32_t unixSeconds, char *out, size_t capacity);

/**
  * @brief Whole days from now until a later time, rounded down.
  * @param untilUnixSeconds (uint32_t) Target time.
  * @param nowUnixSeconds (uint32_t) Current time.
  * @retval (long) Days left; negative once the target time has passed.
  */
long daysUntil(uint32_t untilUnixSeconds, uint32_t nowUnixSeconds);

#endif
