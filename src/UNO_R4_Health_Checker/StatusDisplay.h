/**
  ******************************************************************************
  * @file           : StatusDisplay.h
  * @brief          : Caution sign and heartbeat LED on the LED matrix.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
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

#ifndef STATUS_DISPLAY_H
#define STATUS_DISPLAY_H

#include <Arduino.h>

/*
  Status indication on the UNO R4 WiFi built-in 12 x 8 LED matrix.

  Shows a steady caution sign (a triangle with an exclamation mark) while at
  least one service is failing. Independently, the top-left LED, which the
  sign does not use, shows the heartbeat (see Heartbeat.h).

  The matrix library multiplexes the LEDs from its own timer interrupt, so
  the image stays visible while the main loop is blocked in a health check.
  The caution sign is steady rather than blinking for the same reason: a
  blink driven from loop() would stop during a check.
*/
class StatusDisplay
{
public:
  /**
    * @brief Create a dark display. Nothing is drawn before begin().
    */
  StatusDisplay();

  /**
    * @brief Start the LED matrix driver and clear the display.
    * @retval (bool) False when no hardware timer was available for the
    *         matrix; the display then stays dark.
    */
  bool begin();

  /**
    * @brief Show or hide the caution sign. Redraws only on a change.
    * @param servicesFailing (bool) True while at least one service is
    *        failing.
    */
  void update(bool servicesFailing);

  /**
    * @brief Light or clear the heartbeat LED. Redraws only on a change.
    * @param ledOn (bool) True to light the LED.
    */
  void setHeartbeat(bool ledOn);

private:
  /**
    * @brief Compose the caution sign and heartbeat LED and load the frame.
    */
  void render();

  bool _running;
  bool _cautionShown;
  bool _heartbeatLedOn;
};

#endif
