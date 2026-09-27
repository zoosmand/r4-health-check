#ifndef TEXT_PARSING_H
#define TEXT_PARSING_H

/*
  Pure text helpers shared by the firmware and the host-side unit tests.

  Nothing in this module depends on Arduino headers, so it can be compiled
  and tested on a development machine (see test/host).
*/

#include <stddef.h>
#include <stdint.h>

/**
  * @brief Extract the status code from an HTTP response status line.
  * @param statusLine (const char*) Non-null line without CR/LF, for example
  *        "HTTP/1.1 200 OK".
  * @retval (int) Status code in the range 100..599, or 0 when the line is
  *         not a valid HTTP status line.
  */
int parseHttpStatusCode(const char *statusLine);

/**
  * @brief Split an HTTP request line into method and path.
  * @param requestLine (const char*) Non-null line without CR/LF, for example
  *        "GET /api/status?x=1 HTTP/1.1".
  * @param method (char*) Non-null output buffer for the method.
  * @param methodCapacity (size_t) Size of method in bytes, including NUL.
  * @param path (char*) Non-null output buffer for the path. The query string
  *        and fragment are removed.
  * @param pathCapacity (size_t) Size of path in bytes, including NUL.
  * @retval (bool) True when the line has three space-separated parts, begins
  *         with an origin-form path, and both parts fit their buffers.
  */
bool parseHttpRequestLine(
  const char *requestLine,
  char *method,
  size_t methodCapacity,
  char *path,
  size_t pathCapacity
);

/**
  * @brief Test whether a header line has the given name, ignoring case.
  * @param headerLine (const char*) Non-null line such as "Host: example".
  * @param name (const char*) Non-null header name without the colon.
  * @retval (const char*) Pointer to the header value with leading blanks
  *         skipped, or nullptr when the name does not match.
  */
const char *matchHttpHeader(const char *headerLine, const char *name);

/**
  * @brief Compare two secrets in time independent of the first mismatch.
  * @param expected (const char*) Non-null expected secret.
  * @param actual (const char*) Non-null candidate value.
  * @retval (bool) True when both strings are identical.
  */
bool constantTimeEquals(const char *expected, const char *actual);

/**
  * @brief Compare dotted numeric version strings such as "0.4.10".
  * @param left (const char*) Non-null version string.
  * @param right (const char*) Non-null version string.
  * @retval (int) Negative, zero, or positive when left is lower than, equal
  *         to, or higher than right. Missing components count as zero.
  */
int compareVersions(const char *left, const char *right);

/**
  * @brief Produce the JSON string escape sequence for one character.
  * @param c (char) Character to escape.
  * @param out (char*) Non-null buffer of at least 7 bytes. Receives the
  *        NUL-terminated escaped form (the character itself when no escape
  *        is needed).
  * @retval (size_t) Length of the text written to out, excluding NUL.
  */
size_t escapeJsonChar(char c, char *out);

#endif
