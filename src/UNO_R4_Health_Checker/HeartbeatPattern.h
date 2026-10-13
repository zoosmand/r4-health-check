/**
  ******************************************************************************
  * @file           : HeartbeatPattern.h
  * @brief          : Timing of the heartbeat LED's double pulse.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 28.09.2026
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
