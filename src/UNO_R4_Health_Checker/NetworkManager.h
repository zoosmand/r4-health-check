#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "WiFiS3.h"

/*
  Wi-Fi station connection with periodic reconnection and outage tracking.

  The WiFiS3 library reports WL_CONNECTED as soon as the station has joined
  the access point, before DHCP has assigned an address. This class treats
  the network as ready only when the station is associated AND has a non-zero
  IP address, and announces the address once it is known.

  isConnected() and localIp() return values cached by update(), so callers
  can query them repeatedly without a Wi-Fi module round trip.
*/
class NetworkManager
{
public:
  /**
    * @param ssid (const char*) Non-null network name; must outlive this object.
    * @param password (const char*) Non-null passphrase; must outlive this object.
    * @param reconnectIntervalMs (unsigned long) Minimum time between attempts.
    * @param connectTimeoutMs (unsigned long) How long one attempt blocks.
    * @param addressTimeoutMs (unsigned long) How long to wait for a DHCP
    *        address after association before reconnecting.
    * @param outageAlarmMs (unsigned long) Time without a ready network after
    *        which isOutageAlarmDue() becomes true.
    */
  NetworkManager(
    const char *ssid,
    const char *password,
    unsigned long reconnectIntervalMs,
    unsigned long connectTimeoutMs,
    unsigned long addressTimeoutMs,
    unsigned long outageAlarmMs
  );

  /**
    * @brief Check the Wi-Fi module and make the first connection attempt.
    *        Blocks for up to connectTimeoutMs.
    * @retval (bool) False when the Wi-Fi module does not respond.
    */
  bool begin();

  /**
    * @brief Refresh the cached state and reconnect when due. May block for
    *        up to connectTimeoutMs.
    */
  void update();

  /**
    * @brief Report whether the station is associated and has an IP address.
    * @retval (bool) True while the network is usable.
    */
  bool isConnected() const;

  /**
    * @brief Report, once, that the network has become ready since the last
    *        call. Use it to announce the address or restart listeners.
    * @retval (bool) True on the first call after the network became ready.
    */
  bool consumeNetworkReady();

  /**
    * @brief Report whether the network has not been ready for outageAlarmMs.
    * @retval (bool) True while the outage alarm should sound.
    */
  bool isOutageAlarmDue() const;

  /**
    * @brief Time since the network stopped being ready.
    * @retval (unsigned long) Milliseconds, or 0 while connected.
    */
  unsigned long outageDurationMs() const;

  /**
    * @brief Address assigned by DHCP.
    * @retval (IPAddress) Cached address, or 0.0.0.0 while not connected.
    */
  IPAddress localIp() const;

  long rssi() const;
  String ssid() const;

  void printStatus() const;

private:
  void attemptConnection();

  /**
    * @brief Query association and, while needed, the IP address.
    */
  void pollStatus();

  const char *_ssid;
  const char *_password;

  unsigned long _reconnectIntervalMs;
  unsigned long _connectTimeoutMs;
  unsigned long _addressTimeoutMs;
  unsigned long _outageAlarmMs;

  unsigned long _lastReconnectAttemptAtMs;
  unsigned long _lastPollAtMs;
  unsigned long _associatedAtMs;
  unsigned long _disconnectedSinceMs;

  bool _associated;
  bool _connected;
  bool _readyPending;
  IPAddress _localIp;
};

#endif
