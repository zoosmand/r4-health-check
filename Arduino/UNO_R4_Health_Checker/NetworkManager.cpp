#include "NetworkManager.h"

NetworkManager::NetworkManager(
  const char *ssid,
  const char *password,
  unsigned long reconnectIntervalMs
)
  : _ssid(ssid),
    _password(password),
    _reconnectIntervalMs(reconnectIntervalMs),
    _lastReconnectAttemptAtMs(0)
{
}

bool NetworkManager::begin()
{
  if (WiFi.status() == WL_NO_MODULE)
  {
    return false;
  }

  const String firmwareVersion = WiFi.firmwareVersion();

  Serial.print(F("WiFi firmware: "));
  Serial.println(firmwareVersion);

  if (firmwareVersion < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println(F("WARNING: WiFi firmware should be upgraded."));
  }

  attemptConnection();
  return true;
}

void NetworkManager::update()
{
  if (isConnected())
  {
    return;
  }

  const unsigned long now = millis();

  if (now - _lastReconnectAttemptAtMs < _reconnectIntervalMs)
  {
    return;
  }

  attemptConnection();
}

bool NetworkManager::isConnected() const
{
  return WiFi.status() == WL_CONNECTED;
}

IPAddress NetworkManager::localIp() const
{
  return WiFi.localIP();
}

long NetworkManager::rssi() const
{
  return WiFi.RSSI();
}

String NetworkManager::ssid() const
{
  return WiFi.SSID();
}

void NetworkManager::printStatus() const
{
  if (!isConnected())
  {
    Serial.println(F("WiFi is not connected."));
    return;
  }

  Serial.print(F("SSID: "));
  Serial.println(ssid());

  Serial.print(F("IP address: "));
  Serial.println(localIp());

  Serial.print(F("Signal strength: "));
  Serial.print(rssi());
  Serial.println(F(" dBm"));
}

void NetworkManager::attemptConnection()
{
  _lastReconnectAttemptAtMs = millis();

  Serial.print(F("Connecting to SSID: "));
  Serial.println(_ssid);

  WiFi.disconnect();
  WiFi.begin(_ssid, _password);

  if (isConnected())
  {
    Serial.println(F("WiFi connected."));
    printStatus();
  }
  else
  {
    Serial.println(F("WiFi connection attempt failed."));
  }
}
