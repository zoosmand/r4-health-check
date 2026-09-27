#include "NetworkManager.h"
#include "TextParsing.h"

namespace
{
// Each poll is one or two Wi-Fi module commands; no need to run it on every
// loop pass.
constexpr unsigned long STATUS_POLL_INTERVAL_MS = 250UL;
}  // namespace

NetworkManager::NetworkManager(
  const char *ssid,
  const char *password,
  unsigned long reconnectIntervalMs,
  unsigned long connectTimeoutMs,
  unsigned long addressTimeoutMs,
  unsigned long outageAlarmMs
)
  : _ssid(ssid),
    _password(password),
    _reconnectIntervalMs(reconnectIntervalMs),
    _connectTimeoutMs(connectTimeoutMs),
    _addressTimeoutMs(addressTimeoutMs),
    _outageAlarmMs(outageAlarmMs),
    _lastReconnectAttemptAtMs(0),
    _lastPollAtMs(0),
    _associatedAtMs(0),
    _disconnectedSinceMs(0),
    _associated(false),
    _connected(false),
    _readyPending(false),
    _localIp(0, 0, 0, 0)
{
}

bool NetworkManager::begin()
{
  if (WiFi.status() == WL_NO_MODULE)
  {
    return false;
  }

  // WiFi.begin() busy-waits for this long.
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
  const unsigned long now = millis();

  if (now - _lastPollAtMs >= STATUS_POLL_INTERVAL_MS)
  {
    pollStatus();
  }

  if (_connected || now - _lastReconnectAttemptAtMs < _reconnectIntervalMs)
  {
    return;
  }

  // Associated but still waiting for DHCP: give it time before starting over.
  if (_associated && now - _associatedAtMs < _addressTimeoutMs)
  {
    return;
  }

  attemptConnection();
}

bool NetworkManager::isConnected() const
{
  return _connected;
}

bool NetworkManager::consumeNetworkReady()
{
  const bool pending = _readyPending;
  _readyPending = false;
  return pending;
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
  return _localIp;
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
  Serial.println(_localIp);

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
  _associated = false;

  if (WiFi.begin(_ssid, _password) != WL_CONNECTED)
  {
    Serial.println(F("WiFi not associated yet; retrying in the background."));
  }

  pollStatus();
}

void NetworkManager::pollStatus()
{
  const unsigned long now = millis();
  _lastPollAtMs = now;

  const bool associated = WiFi.status() == WL_CONNECTED;

  if (associated && !_associated)
  {
    _associatedAtMs = now;
    Serial.println(F("WiFi associated; waiting for an IP address."));
  }

  _associated = associated;

  if (!associated)
  {
    if (_connected)
    {
      _disconnectedSinceMs = now;
      Serial.println(F("WiFi connection lost."));
    }

    _connected = false;
    _localIp = IPAddress(0, 0, 0, 0);
    return;
  }

  if (_connected)
  {
    return;
  }

  // DHCP completes after association; WiFi.localIP() is 0.0.0.0 until then.
  const IPAddress address = WiFi.localIP();

  if (address == IPAddress(0, 0, 0, 0))
  {
    return;
  }

  _localIp = address;
  _connected = true;
  _readyPending = true;

  Serial.println(F("WiFi connected."));
  printStatus();
}
