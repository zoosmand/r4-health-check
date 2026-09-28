#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <Arduino.h>
#include "NetworkManager.h"
#include "StatusDisplay.h"
#include "Watchdog.h"

/*
  Heartbeat service.

  While the network is ready, one LED of the matrix beats like a heart. The
  beat is driven from the main loop on purpose: it freezes while the loop is
  blocked, so a steady or dark LED means the firmware is busy or stuck. While
  the network is not ready, the LED stays dark.

  When the network has not been ready for the restart timeout (counted from
  boot as well), the service stops refreshing the watchdog, and the hardware
  watchdog resets the board within one WDT period.
*/
class Heartbeat
{
public:
  /**
    * @param networkManager (NetworkManager&) Source of the outage state; must
    *        outlive this object.
    * @param statusDisplay (StatusDisplay&) Display that shows the heartbeat
    *        LED; must outlive this object.
    * @param watchdog (Watchdog&) Watchdog asked to reset the board; must
    *        outlive this object.
    * @param periodMs (unsigned long) Time of one beat; at least
    *        HEARTBEAT_MINIMUM_PERIOD_MS.
    * @param outageRestartMs (unsigned long) Time without a ready network
    *        after which the board is restarted.
    */
  Heartbeat(
    NetworkManager &networkManager,
    StatusDisplay &statusDisplay,
    Watchdog &watchdog,
    unsigned long periodMs,
    unsigned long outageRestartMs
  );

  /**
    * @brief Advance the beat and restart the board when the outage has
    *        lasted too long. Non-blocking; call from loop() after
    *        NetworkManager::update().
    */
  void update();

  /**
    * @brief Report whether a watchdog restart has been requested.
    * @retval (bool) True once the outage timeout has been reached.
    */
  bool isRestartRequested() const;

private:
  NetworkManager &_networkManager;
  StatusDisplay &_statusDisplay;
  Watchdog &_watchdog;

  unsigned long _periodMs;
  unsigned long _outageRestartMs;

  bool _restartRequested;
};

#endif
