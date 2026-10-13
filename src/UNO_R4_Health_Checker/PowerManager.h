/**
  ******************************************************************************
  * @file           : PowerManager.h
  * @brief          : RA4M1 Sleep mode between main loop passes.
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

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "Watchdog.h"

/*
  Idle power saving for the RA4M1.

  Between main loop passes the CPU waits in Sleep mode (WFI): the CPU clock
  stops, while peripherals, timers, and interrupts keep running. The 1 ms
  millis() tick, the buzzer and watchdog supervisor timers, the LED matrix,
  and the UART and USB interrupts each wake the CPU, so nothing is missed.

  Software Standby is not used: it stops the clocks that millis(), the LED
  matrix, and the Wi-Fi UART depend on.

  The Arduino WDT library configures the hardware watchdog to stop counting
  in Sleep mode. The supervisor interrupt keeps its own time, so a stuck main
  loop is still detected, but a deliberate reset (Watchdog::requestReset())
  would be delayed by the time spent asleep. Sleeping therefore stops once a
  reset has been requested.

  The Wi-Fi coprocessor has its own power management; WiFiS3 provides no
  control over it.
*/
class PowerManager
{
public:
  /**
    * @param watchdog (Watchdog&) Watchdog whose reset request disables
    *        sleeping; must outlive this object.
    */
  explicit PowerManager(Watchdog &watchdog);

  /**
    * @brief Enable idle sleep when WFI enters Sleep mode rather than
    *        Software Standby.
    * @retval (bool) True when idle() will sleep.
    */
  bool begin();

  /**
    * @brief Wait in Sleep mode for durationMs, waking on every interrupt.
    *        Blocks for up to durationMs. Returns at once when sleep is not
    *        enabled or a watchdog reset has been requested. Call from thread
    *        context only.
    * @param durationMs (unsigned long) Time to idle.
    */
  void idle(unsigned long durationMs);

  /**
    * @brief Report whether idle() sleeps.
    * @retval (bool) The result of begin().
    */
  bool isEnabled() const;

private:
  Watchdog &_watchdog;
  bool _enabled;
};

#endif
