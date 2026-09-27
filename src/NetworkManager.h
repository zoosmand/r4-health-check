#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "WiFiS3.h"

class NetworkManager
{
public:
  NetworkManager(
    const char *ssid,
    const char *password,
    unsigned long reconnectIntervalMs
  );

  bool begin();
  void update();

  bool isConnected() const;
  IPAddress localIp() const;
  long rssi() const;
  String ssid() const;

  void printStatus() const;

private:
  void attemptConnection();

  const char *_ssid;
  const char *_password;

  unsigned long _reconnectIntervalMs;
  unsigned long _lastReconnectAttemptAtMs;
};

#endif
