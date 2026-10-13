#include "ResourceAlert.h"

namespace
{
/**
  * @brief Length of the beeps for one resource, without the pause before.
  */
unsigned long getCountDurationMs(
  const ResourceAlertPattern &pattern,
  uint8_t number
)
{
  return static_cast<unsigned long>(number) * pattern.beepMs +
         static_cast<unsigned long>(number - 1U) * pattern.beepGapMs;
}
}  // namespace

unsigned long getResourceAlertDurationMs(
  const ResourceAlertPattern &pattern,
  uint8_t resourceMask
)
{
  unsigned long total = getMelodyDurationMs(pattern.prefix, pattern.prefixCount);

  for (uint8_t number = 1; number <= MAX_RESOURCE_NUMBER; number++)
  {
    if ((resourceMask & getResourceBit(number)) != 0)
    {
      total += pattern.pauseMs + getCountDurationMs(pattern, number);
    }
  }

  return total;
}

bool findResourceAlertFrequency(
  const ResourceAlertPattern &pattern,
  uint8_t resourceMask,
  unsigned long positionMs,
  uint16_t &frequencyHz
)
{
  frequencyHz = 0;

  const unsigned long prefixMs =
    getMelodyDurationMs(pattern.prefix, pattern.prefixCount);

  if (positionMs < prefixMs)
  {
    return findMelodyFrequency(
      pattern.prefix,
      pattern.prefixCount,
      positionMs,
      frequencyHz
    );
  }

  positionMs -= prefixMs;

  for (uint8_t number = 1; number <= MAX_RESOURCE_NUMBER; number++)
  {
    if ((resourceMask & getResourceBit(number)) == 0)
    {
      continue;
    }

    if (positionMs < pattern.pauseMs)
    {
      return true;
    }

    positionMs -= pattern.pauseMs;

    const unsigned long countMs = getCountDurationMs(pattern, number);

    if (positionMs < countMs)
    {
      const unsigned long beepPeriodMs =
        static_cast<unsigned long>(pattern.beepMs) + pattern.beepGapMs;

      if (positionMs % beepPeriodMs < pattern.beepMs)
      {
        frequencyHz = pattern.beepHz;
      }

      return true;
    }

    positionMs -= countMs;
  }

  return false;
}

ResourceAlertSchedule::ResourceAlertSchedule(
  const ResourceAlertPattern &pattern,
  unsigned long intervalMs
)
  : _pattern(pattern),
    _intervalMs(intervalMs),
    _scheduled(false),
    _startedAtMs(0),
    _announcedMask(0)
{
}

bool ResourceAlertSchedule::update(
  uint8_t affectedMask,
  unsigned long nowMs,
  uint16_t &frequencyHz
)
{
  frequencyHz = 0;

  if (affectedMask == 0)
  {
    _scheduled = false;
    _announcedMask = 0;
    return false;
  }

  if (_scheduled)
  {
    const unsigned long elapsedMs = nowMs - _startedAtMs;

    if (findResourceAlertFrequency(_pattern, _announcedMask, elapsedMs, frequencyHz))
    {
      return true;
    }

    // Forget resources that have recovered, so they are announced at once
    // if they are affected again.
    _announcedMask &= affectedMask;

    const bool newResource = (affectedMask & ~_announcedMask) != 0;

    if (!newResource && elapsedMs < _intervalMs)
    {
      return false;
    }
  }

  _scheduled = true;
  _startedAtMs = nowMs;
  _announcedMask = affectedMask;

  return findResourceAlertFrequency(_pattern, _announcedMask, 0, frequencyHz);
}
