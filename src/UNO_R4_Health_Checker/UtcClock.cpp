#include "UtcClock.h"

namespace
{
// 2026-01-01T00:00:00Z.
constexpr uint32_t MINIMUM_VALID_UNIX_SECONDS = 1767225600UL;
}  // namespace

UtcClock::UtcClock()
  : _set(false),
    _unixSecondsAtSet(0),
    _setAtMs(0)
{
}

bool UtcClock::set(uint32_t unixSeconds)
{
  if (unixSeconds < MINIMUM_VALID_UNIX_SECONDS)
  {
    return false;
  }

  _unixSecondsAtSet = unixSeconds;
  _setAtMs = millis();
  _set = true;
  return true;
}

bool UtcClock::isSet() const
{
  return _set;
}

uint32_t UtcClock::nowUnixSeconds() const
{
  if (!_set)
  {
    return 0;
  }

  return _unixSecondsAtSet + (millis() - _setAtMs) / 1000UL;
}
