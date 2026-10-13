/**
  ******************************************************************************
  * @file           : text_parsing_test.cpp
  * @brief          : Host tests for TextParsing (test/host/run.sh).
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Host computer (C++17 unit tests)
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

#include "../../src/UNO_R4_Health_Checker/TextParsing.h"

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

/**
  * @brief Valid and malformed HTTP status lines.
  */
static void testParseHttpStatusCode()
{
  EXPECT(parseHttpStatusCode("HTTP/1.1 200 OK") == 200);
  EXPECT(parseHttpStatusCode("HTTP/1.0 404 Not Found") == 404);
  EXPECT(parseHttpStatusCode("HTTP/2 301") == 301);
  EXPECT(parseHttpStatusCode("HTTP/1.1 599 Custom") == 599);

  EXPECT(parseHttpStatusCode("") == 0);
  EXPECT(parseHttpStatusCode("HTTP/1.1") == 0);
  EXPECT(parseHttpStatusCode("HTTP/1.1 ") == 0);
  EXPECT(parseHttpStatusCode("HTTP/1.1 20") == 0);
  EXPECT(parseHttpStatusCode("HTTP/1.1 2000 Too long") == 0);
  EXPECT(parseHttpStatusCode("HTTP/1.1 abc") == 0);
  EXPECT(parseHttpStatusCode("HTTP/1.1 099 Too low") == 0);
  EXPECT(parseHttpStatusCode("HTTP/1.1 600 Too high") == 0);
  EXPECT(parseHttpStatusCode("SSH-2.0-OpenSSH") == 0);
}

/**
  * @brief Request line parsing, removal of query strings and fragments,
  *        malformed
  *        lines, and the path buffer limit.
  */
static void testParseHttpRequestLine()
{
  char method[8];
  char path[32];

  EXPECT(parseHttpRequestLine("GET /api/status HTTP/1.1",
                              method, sizeof(method), path, sizeof(path)));
  EXPECT(std::strcmp(method, "GET") == 0);
  EXPECT(std::strcmp(path, "/api/status") == 0);

  EXPECT(parseHttpRequestLine("POST /api/check?now=1 HTTP/1.1",
                              method, sizeof(method), path, sizeof(path)));
  EXPECT(std::strcmp(method, "POST") == 0);
  EXPECT(std::strcmp(path, "/api/check") == 0);

  EXPECT(parseHttpRequestLine("GET /a#frag HTTP/1.1",
                              method, sizeof(method), path, sizeof(path)));
  EXPECT(std::strcmp(path, "/a") == 0);

  EXPECT(!parseHttpRequestLine("GET", method, sizeof(method), path, sizeof(path)));
  EXPECT(!parseHttpRequestLine("GET /x", method, sizeof(method), path, sizeof(path)));
  EXPECT(!parseHttpRequestLine(" /x HTTP/1.1", method, sizeof(method), path, sizeof(path)));
  EXPECT(!parseHttpRequestLine("GET  HTTP/1.1", method, sizeof(method), path, sizeof(path)));
  EXPECT(!parseHttpRequestLine("GET http://h/x HTTP/1.1",
                               method, sizeof(method), path, sizeof(path)));
  EXPECT(!parseHttpRequestLine("PROPPATCHX /x HTTP/1.1",
                               method, sizeof(method), path, sizeof(path)));
  EXPECT(!parseHttpRequestLine("GET /0123456789012345678901234567890123 HTTP/1.1",
                               method, sizeof(method), path, sizeof(path)));
}

/**
  * @brief Case-insensitive header names, a tab after the colon, and near-miss
  *        names.
  */
static void testMatchHttpHeader()
{
  const char *value = matchHttpHeader("Authorization: Bearer abc", "Authorization");
  EXPECT(value != nullptr && std::strcmp(value, "Bearer abc") == 0);

  value = matchHttpHeader("authorization:\tBearer abc", "Authorization");
  EXPECT(value != nullptr && std::strcmp(value, "Bearer abc") == 0);

  EXPECT(matchHttpHeader("Authorizations: x", "Authorization") == nullptr);
  EXPECT(matchHttpHeader("Author: x", "Authorization") == nullptr);
  EXPECT(matchHttpHeader("", "Authorization") == nullptr);
}

/**
  * @brief Equal, different, and different-length strings.
  */
static void testConstantTimeEquals()
{
  EXPECT(constantTimeEquals("secret", "secret"));
  EXPECT(!constantTimeEquals("secret", "secreT"));
  EXPECT(!constantTimeEquals("secret", "secre"));
  EXPECT(!constantTimeEquals("secret", "secrets"));
  EXPECT(!constantTimeEquals("secret", ""));
  EXPECT(constantTimeEquals("", ""));
}

/**
  * @brief Dotted version ordering, including missing components.
  */
static void testCompareVersions()
{
  EXPECT(compareVersions("0.6.0", "0.6.0") == 0);
  EXPECT(compareVersions("0.6", "0.6.0") == 0);
  EXPECT(compareVersions("0.4.1", "0.6.0") < 0);
  EXPECT(compareVersions("0.10.0", "0.6.0") > 0);  // String compare gets this wrong.
  EXPECT(compareVersions("1.0.0", "0.99.99") > 0);
  EXPECT(compareVersions("0.6.0", "0.6.1") < 0);
}

/**
  * @brief Digit-only numbers, range limits, overflow, and rejected characters.
  */
static void testParseDecimalNumber()
{
  unsigned long value = 99;

  EXPECT(parseDecimalNumber("1", 5, value) && value == 1);
  EXPECT(parseDecimalNumber("5", 5, value) && value == 5);
  EXPECT(parseDecimalNumber("0", 5, value) && value == 0);
  EXPECT(parseDecimalNumber("007", 10, value) && value == 7);
  EXPECT(parseDecimalNumber("4294967295", 4294967295UL, value) &&
         value == 4294967295UL);

  value = 99;
  EXPECT(!parseDecimalNumber("6", 5, value));
  EXPECT(!parseDecimalNumber("", 5, value));
  EXPECT(!parseDecimalNumber("-1", 5, value));
  EXPECT(!parseDecimalNumber("+1", 5, value));
  EXPECT(!parseDecimalNumber(" 1", 5, value));
  EXPECT(!parseDecimalNumber("1/", 5, value));
  EXPECT(!parseDecimalNumber("1a", 5, value));
  EXPECT(!parseDecimalNumber("4294967296", 4294967295UL, value));
  EXPECT(!parseDecimalNumber("99999999999999999999", 4294967295UL, value));
  EXPECT(value == 99);
}

/**
  * @brief JSON escapes for quotes, backslashes, and control characters.
  */
static void testEscapeJsonChar()
{
  char out[7];

  EXPECT(escapeJsonChar('a', out) == 1 && std::strcmp(out, "a") == 0);
  EXPECT(escapeJsonChar('"', out) == 2 && std::strcmp(out, "\\\"") == 0);
  EXPECT(escapeJsonChar('\\', out) == 2 && std::strcmp(out, "\\\\") == 0);
  EXPECT(escapeJsonChar('\n', out) == 2 && std::strcmp(out, "\\n") == 0);
  EXPECT(escapeJsonChar('\x01', out) == 6 && std::strcmp(out, "\\u0001") == 0);
  EXPECT(escapeJsonChar('\x1f', out) == 6 && std::strcmp(out, "\\u001f") == 0);
}

/**
  * @brief Run every test and print the totals.
  * @retval (int) 0 when every expectation held, 1 otherwise.
  */
int main()
{
  testParseHttpStatusCode();
  testParseHttpRequestLine();
  testMatchHttpHeader();
  testConstantTimeEquals();
  testCompareVersions();
  testEscapeJsonChar();
  testParseDecimalNumber();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
