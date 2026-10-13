/**
  ******************************************************************************
  * @file           : TextParsing.cpp
  * @brief          : Arduino-independent HTTP, JSON, and number parsing.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 27.09.2026
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

#include "TextParsing.h"

#include <string.h>

namespace
{
/**
  * @brief ASCII-only lower-case conversion, independent of the C locale.
  */
char toLowerAscii(char c)
{
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/**
  * @brief Copy [begin, end) into a NUL-terminated buffer.
  * @retval (bool) False when the text does not fit.
  */
bool copyRange(
  const char *begin,
  const char *end,
  char *out,
  size_t capacity
)
{
  const size_t length = static_cast<size_t>(end - begin);

  if (capacity == 0 || length >= capacity)
  {
    return false;
  }

  memcpy(out, begin, length);
  out[length] = '\0';
  return true;
}
}  // namespace

int parseHttpStatusCode(const char *statusLine)
{
  if (strncmp(statusLine, "HTTP/", 5) != 0)
  {
    return 0;
  }

  const char *space = strchr(statusLine, ' ');

  if (space == nullptr)
  {
    return 0;
  }

  const char *digits = space + 1;
  int code = 0;

  for (int i = 0; i < 3; i++)
  {
    if (digits[i] < '0' || digits[i] > '9')
    {
      return 0;
    }

    code = code * 10 + (digits[i] - '0');
  }

  // The code must be followed by the reason phrase or the end of the line.
  if (digits[3] != '\0' && digits[3] != ' ')
  {
    return 0;
  }

  return (code >= 100 && code <= 599) ? code : 0;
}

bool parseHttpRequestLine(
  const char *requestLine,
  char *method,
  size_t methodCapacity,
  char *path,
  size_t pathCapacity
)
{
  const char *firstSpace = strchr(requestLine, ' ');

  if (firstSpace == nullptr || firstSpace == requestLine)
  {
    return false;
  }

  const char *target = firstSpace + 1;
  const char *secondSpace = strchr(target, ' ');

  if (secondSpace == nullptr || secondSpace == target || *target != '/')
  {
    return false;
  }

  const char *targetEnd = target;

  while (targetEnd < secondSpace && *targetEnd != '?' && *targetEnd != '#')
  {
    targetEnd++;
  }

  return copyRange(requestLine, firstSpace, method, methodCapacity) &&
         copyRange(target, targetEnd, path, pathCapacity);
}

const char *matchHttpHeader(const char *headerLine, const char *name)
{
  size_t i = 0;

  for (; name[i] != '\0'; i++)
  {
    if (toLowerAscii(headerLine[i]) != toLowerAscii(name[i]))
    {
      return nullptr;
    }
  }

  if (headerLine[i] != ':')
  {
    return nullptr;
  }

  const char *value = headerLine + i + 1;

  while (*value == ' ' || *value == '\t')
  {
    value++;
  }

  return value;
}

bool constantTimeEquals(const char *expected, const char *actual)
{
  const size_t expectedLength = strlen(expected);
  const size_t actualLength = strlen(actual);

  unsigned char difference = (expectedLength == actualLength) ? 0 : 1;

  // Always walk the whole expected secret so timing does not reveal a prefix.
  for (size_t i = 0; i < expectedLength; i++)
  {
    const char candidate = (i < actualLength) ? actual[i] : '\0';
    difference |= static_cast<unsigned char>(expected[i] ^ candidate);
  }

  return difference == 0;
}

namespace
{
/**
  * @brief Read one numeric version component and advance past the next dot.
  */
unsigned long nextVersionComponent(const char *&text)
{
  unsigned long value = 0;

  while (*text >= '0' && *text <= '9')
  {
    value = value * 10UL + static_cast<unsigned long>(*text - '0');
    text++;
  }

  // Skip anything else up to and including the next separator.
  while (*text != '\0' && *text != '.')
  {
    text++;
  }

  if (*text == '.')
  {
    text++;
  }

  return value;
}
}  // namespace

int compareVersions(const char *left, const char *right)
{
  while (*left != '\0' || *right != '\0')
  {
    const unsigned long leftPart = nextVersionComponent(left);
    const unsigned long rightPart = nextVersionComponent(right);

    if (leftPart != rightPart)
    {
      return leftPart < rightPart ? -1 : 1;
    }
  }

  return 0;
}

size_t escapeJsonChar(char c, char *out)
{
  static const char HEX_DIGITS[] = "0123456789abcdef";

  switch (c)
  {
    case '"':  strcpy(out, "\\\""); return 2;
    case '\\': strcpy(out, "\\\\"); return 2;
    case '\n': strcpy(out, "\\n");  return 2;
    case '\r': strcpy(out, "\\r");  return 2;
    case '\t': strcpy(out, "\\t");  return 2;
    default:
      break;
  }

  const unsigned char code = static_cast<unsigned char>(c);

  if (code < 0x20)
  {
    out[0] = '\\';
    out[1] = 'u';
    out[2] = '0';
    out[3] = '0';
    out[4] = HEX_DIGITS[code >> 4];
    out[5] = HEX_DIGITS[code & 0x0F];
    out[6] = '\0';
    return 6;
  }

  out[0] = c;
  out[1] = '\0';
  return 1;
}

bool parseDecimalNumber(const char *text, unsigned long maxValue, unsigned long &value)
{
  if (text[0] == '\0')
  {
    return false;
  }

  unsigned long result = 0;

  for (const char *p = text; *p != '\0'; p++)
  {
    if (*p < '0' || *p > '9')
    {
      return false;
    }

    const unsigned long digit = static_cast<unsigned long>(*p - '0');

    // Checked before multiplying, so the value never overflows.
    if (digit > maxValue || result > (maxValue - digit) / 10UL)
    {
      return false;
    }

    result = result * 10UL + digit;
  }

  value = result;
  return true;
}
