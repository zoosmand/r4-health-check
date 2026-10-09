#ifndef BUZZER_MELODY_H
#define BUZZER_MELODY_H

/*
  Melody description and lookup for the passive buzzer, kept free of
  Arduino headers so it can be tested on the host.
*/

#include <stddef.h>
#include <stdint.h>

/**
  * @brief One note of a melody.
  * @param frequencyHz (uint16_t) Tone frequency, or 0 for a rest.
  * @param durationMs (uint16_t) Length of the note. The buzzer pattern timer
  *        runs at 100 Hz, so use multiples of 10 ms.
  */
struct MelodyNote
{
  uint16_t frequencyHz;
  uint16_t durationMs;
};

/**
  * @brief Total length of a melody.
  * @param notes (const MelodyNote*) Notes; may be nullptr when count is 0.
  * @param count (size_t) Number of notes.
  * @retval (unsigned long) Sum of all note durations in milliseconds.
  */
unsigned long getMelodyDurationMs(const MelodyNote *notes, size_t count);

/**
  * @brief Find what sounds at a position within a melody. Safe to call from
  *        an interrupt: it does not allocate and runs in O(count).
  * @param notes (const MelodyNote*) Notes; may be nullptr when count is 0.
  * @param count (size_t) Number of notes.
  * @param positionMs (unsigned long) Time since the melody started.
  * @param frequencyHz (uint16_t&) Receives the note frequency, or 0 during
  *        a rest.
  * @retval (bool) False once positionMs is past the end of the melody.
  */
bool findMelodyFrequency(
  const MelodyNote *notes,
  size_t count,
  unsigned long positionMs,
  uint16_t &frequencyHz
);

#endif
