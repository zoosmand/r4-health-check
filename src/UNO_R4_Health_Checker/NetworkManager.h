#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "WiFiS3.h"

/*
  Wi-Fi station connection with periodic reconnection and outage tracking.

  isConnected() returns the status cached by the last update(), so callers
  can query it repeatedly without a Wi-Fi module round trip.
*/
class NetworkManager
{
public:
  /**
    * @param ssid (const char*) Non-null network name; must outlive this object.
    * @param password (const char*) Non-null passphrase; must outlive this object.
    * @param reconnectIntervalMs (unsigned long) Minimum time between attempts.
    * @param connectTimeoutMs (unsigned long) How long one attempt blocks;
    *        must stay below the watchdog timeout.
    * @param outageAlarmMs (unsigned long) Disconnection time after which
    *        isOutageAlarmDue() becomes true.
    */
  NetworkManager(
    const char *ssid,
    const char *password,
    unsigned long reconnectIntervalMs,
    unsigned long connectTimeoutMs,
    unsigned long outageAlarmMs
  );

  /**
    * @brief Check the Wi-Fi module and make the first connection attempt.
    *        Blocks for up to connectTimeoutMs.
    * @retval (bool) False when the Wi-Fi module does not respond.
    */
  bool begin();

  /**
    * @brief Refresh the cached status and reconnect when due. May block for
    *        up to connectTimeoutMs.
    */
  void update();

  bool isConnected() const;

  /**
    * @brief Report whether Wi-Fi has been down for at least outageAlarmMs.
    * @retval (bool) True while the outage alarm should sound.
    */
  bool isOutageAlarmDue() const;

  /**
    * @brief Time since the connection was lost.
    * @retval (unsigned long) Milliseconds, or 0 while connected.
    */
  unsigned long outageDurationMs() const;

  IPAddress localIp() const;
  long rssi() const;
  String ssid() const;

  void printStatus() const;

private:
  void attemptConnection();
  void refreshStatus();

  const char *_ssid;
  const char *_password;

  unsigned long _reconnectIntervalMs;
  unsigned long _connectTimeoutMs;
  unsigned long _outageAlarmMs;

  unsigned long _lastReconnectAttemptAtMs;
  unsigned long _disconnectedSinceMs;
  bool _connected;
};

#endif
