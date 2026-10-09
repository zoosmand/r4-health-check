#include "BuzzerMelody.h"

unsigned long getMelodyDurationMs(const MelodyNote *notes, size_t count)
{
  unsigned long total = 0;

  for (size_t i = 0; i < count; i++)
  {
    total += notes[i].durationMs;
  }

  return total;
}

bool findMelodyFrequency(
  const MelodyNote *notes,
  size_t count,
  unsigned long positionMs,
  uint16_t &frequencyHz
)
{
  for (size_t i = 0; i < count; i++)
  {
    if (positionMs < notes[i].durationMs)
    {
      frequencyHz = notes[i].frequencyHz;
      return true;
    }

    positionMs -= notes[i].durationMs;
  }

  frequencyHz = 0;
  return false;
}
