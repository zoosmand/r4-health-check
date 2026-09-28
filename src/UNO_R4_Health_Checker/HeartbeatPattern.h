#ifndef HEARTBEAT_PATTERN_H
#define HEARTBEAT_PATTERN_H

/*
  Timing of the heartbeat LED: a double "lub-dub" pulse at the start of every
  period, then a pause.

  Nothing in this module depends on Arduino headers, so it can be compiled
  and tested on the host.
*/

// Shortest period that fits both pulses and a visible pause.
constexpr unsigned long HEARTBEAT_MINIMUM_PERIOD_MS = 500UL;

/**
  * @brief Report whether the heartbeat LED is lit at a point in time.
  * @param elapsedMs (unsigned long) Time since any fixed reference; only its
  *        position within the period matters, so millis() wrap-around is
  *        harmless.
  * @param periodMs (unsigned long) Time of one beat; at least
  *        HEARTBEAT_MINIMUM_PERIOD_MS.
  * @retval (bool) True while one of the two pulses is lit. False when periodMs
  *         is below the minimum.
  */
bool isHeartbeatLedOn(unsigned long elapsedMs, unsigned long periodMs);

#endif
