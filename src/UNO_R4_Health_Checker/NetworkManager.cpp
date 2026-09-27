#include "NetworkManager.h"
#include "TextParsing.h"

NetworkManager::NetworkManager(
  const char *ssid,
  const char *password,
  unsigned long reconnectIntervalMs,
  unsigned long connectTimeoutMs,
  unsigned long outageAlarmMs
)
  : _ssid(ssid),
    _password(password),
    _reconnectIntervalMs(reconnectIntervalMs),
    _connectTimeoutMs(connectTimeoutMs),
    _outageAlarmMs(outageAlarmMs),
    _lastReconnectAttemptAtMs(0),
    _disconnectedSinceMs(0),
    _connected(false)
{
}

bool NetworkManager::begin()
{
  if (WiFi.status() == WL_NO_MODULE)
  {
    return false;
  }

  // WiFi.begin() busy-waits for this long; keep it below the watchdog.
  WiFi.setTimeout(_connectTimeoutMs);

  const char *firmwareVersion = WiFi.firmwareVersion();

  Serial.print(F("WiFi firmware: "));
  Serial.println(firmwareVersion);

  if (compareVersions(firmwareVersion, WIFI_FIRMWARE_LATEST_VERSION) < 0)
  {
    Serial.print(F("WARNING: WiFi firmware should be upgraded to "));
    Serial.println(F(WIFI_FIRMWARE_LATEST_VERSION));
  }

  _disconnectedSinceMs = millis();
  attemptConnection();
  return true;
}

void NetworkManager::update()
{
  refreshStatus();

  if (_connected)
  {
    return;
  }

  if (millis() - _lastReconnectAttemptAtMs < _reconnectIntervalMs)
  {
    return;
  }

  attemptConnection();
}

bool NetworkManager::isConnected() const
{
  return _connected;
}

bool NetworkManager::isOutageAlarmDue() const
{
  return !_connected && outageDurationMs() >= _outageAlarmMs;
}

unsigned long NetworkManager::outageDurationMs() const
{
  return _connected ? 0 : millis() - _disconnectedSinceMs;
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
  if (!_connected)
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

  refreshStatus();

  if (_connected)
  {
    Serial.println(F("WiFi connected."));
    printStatus();
  }
  else
  {
    Serial.println(F("WiFi connection attempt failed."));
  }
}

void NetworkManager::refreshStatus()
{
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (_connected && !connected)
  {
    _disconnectedSinceMs = millis();
    Serial.println(F("WiFi connection lost."));
  }

  _connected = connected;
}
