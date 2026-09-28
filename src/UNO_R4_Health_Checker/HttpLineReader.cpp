#include "HttpLineReader.h"

LineReadResult readHttpLine(
  Client &client,
  char *buffer,
  size_t capacity,
  unsigned long timeoutMs,
  Watchdog &watchdog
)
{
  const unsigned long startedAt = millis();
  size_t length = 0;
  bool overflow = false;

  buffer[0] = '\0';

  while (millis() - startedAt < timeoutMs)
  {
    // Refresh on every pass: a peer may stream bytes without a newline for
    // the whole timeout, which can exceed the watchdog period.
    watchdog.refresh();

    if (client.available() <= 0)
    {
      if (!client.connected())
      {
        return LineReadResult::CLOSED;
      }

      delay(1);
      continue;
    }

    const int value = client.read();

    if (value < 0)
    {
      continue;
    }

    const char c = static_cast<char>(value);

    if (c == '\n')
    {
      // Drop the CR of a CRLF terminator.
      if (length > 0 && buffer[length - 1] == '\r')
      {
        buffer[--length] = '\0';
      }

      return overflow ? LineReadResult::TRUNCATED : LineReadResult::COMPLETE;
    }

    if (length + 1 < capacity)
    {
      buffer[length++] = c;
      buffer[length] = '\0';
    }
    else
    {
      overflow = true;
    }
  }

  return LineReadResult::TIMED_OUT;
}
