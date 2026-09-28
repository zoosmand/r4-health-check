#include "Heartbeat.h"
#include "HeartbeatPattern.h"

Heartbeat::Heartbeat(
  NetworkManager &networkManager,
  StatusDisplay &statusDisplay,
  Watchdog &watchdog,
  unsigned long periodMs,
  unsigned long outageRestartMs
)
  : _networkManager(networkManager),
    _statusDisplay(statusDisplay),
    _watchdog(watchdog),
    _periodMs(periodMs),
    _outageRestartMs(outageRestartMs),
    _restartRequested(false),
    _busy(false)
{
}

void Heartbeat::update()
{
  if (_restartRequested)
  {
    return;
  }

  if (!_networkManager.isConnected())
  {
    _statusDisplay.setHeartbeat(false);

    const unsigned long outageMs = _networkManager.outageDurationMs();

    if (outageMs >= _outageRestartMs)
    {
      _restartRequested = true;

      Serial.print(F("Network not ready for "));
      Serial.print(outageMs / 1000UL);
      Serial.println(F(" s; restarting through the watchdog."));

      _watchdog.requestReset();
    }

    return;
  }

  _statusDisplay.setHeartbeat(_busy || isHeartbeatLedOn(millis(), _periodMs));
}

void Heartbeat::setBusy(bool busy)
{
  _busy = busy;

  if (busy && !_restartRequested)
  {
    _statusDisplay.setHeartbeat(true);
  }
}

bool Heartbeat::isRestartRequested() const
{
  return _restartRequested;
}
