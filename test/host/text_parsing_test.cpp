// Host-side unit tests for src/TextParsing.cpp. Run with test/host/run.sh.

#include "../../src/TextParsing.h"

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

static void testConstantTimeEquals()
{
  EXPECT(constantTimeEquals("secret", "secret"));
  EXPECT(!constantTimeEquals("secret", "secreT"));
  EXPECT(!constantTimeEquals("secret", "secre"));
  EXPECT(!constantTimeEquals("secret", "secrets"));
  EXPECT(!constantTimeEquals("secret", ""));
  EXPECT(constantTimeEquals("", ""));
}

static void testCompareVersions()
{
  EXPECT(compareVersions("0.6.0", "0.6.0") == 0);
  EXPECT(compareVersions("0.6", "0.6.0") == 0);
  EXPECT(compareVersions("0.4.1", "0.6.0") < 0);
  EXPECT(compareVersions("0.10.0", "0.6.0") > 0);  // String compare gets this wrong.
  EXPECT(compareVersions("1.0.0", "0.99.99") > 0);
  EXPECT(compareVersions("0.6.0", "0.6.1") < 0);
}

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

int main()
{
  testParseHttpStatusCode();
  testParseHttpRequestLine();
  testMatchHttpHeader();
  testConstantTimeEquals();
  testCompareVersions();
  testEscapeJsonChar();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
