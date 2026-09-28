#include "Watchdog.h"
#include <WDT.h>

namespace
{
// Supervisor rate. Must refresh the WDT several times per hardware period.
constexpr float SUPERVISOR_TICK_HZ = 10.0f;
constexpr uint32_t SUPERVISOR_TICK_MS = 100UL;
}  // namespace

Watchdog::Watchdog()
  : _running(false),
    _supervised(false),
    _hardwareTimeoutMs(0),
    _loopTimeoutMs(0),
    _loopTimeoutTicks(0),
    _ticksSinceRefresh(0),
    _resetRequested(false)
{
}

bool Watchdog::begin(uint32_t hardwareTimeoutMs, uint32_t loopTimeoutMs)
{
  if (_running)
  {
    return true;
  }

  // WDTimer::begin() returns 1 on success and 0 on failure.
  if (WDT.begin(hardwareTimeoutMs) != 1)
  {
    return false;
  }

  _running = true;
  _hardwareTimeoutMs = WDT.getTimeout();
  _loopTimeoutTicks =
    (loopTimeoutMs + SUPERVISOR_TICK_MS - 1) / SUPERVISOR_TICK_MS;
  _ticksSinceRefresh = 0;

  uint8_t timerType = 0;
  const int8_t channel = FspTimer::get_available_timer(timerType);

  _supervised =
    channel >= 0 &&
    _timer.begin(
      TIMER_MODE_PERIODIC,
      timerType,
      static_cast<uint8_t>(channel),
      SUPERVISOR_TICK_HZ,
      0.0f,
      timerCallback,
      this
    ) &&
    _timer.setup_overflow_irq() &&
    _timer.open() &&
    _timer.start();

  _loopTimeoutMs = _supervised ? loopTimeoutMs : _hardwareTimeoutMs;
  return true;
}

void Watchdog::refresh()
{
  if (!_running || _resetRequested)
  {
    return;
  }

  if (_supervised)
  {
    _ticksSinceRefresh = 0;
  }
  else
  {
    WDT.refresh();
  }
}

void Watchdog::requestReset()
{
  _resetRequested = true;

  if (!_running)
  {
    NVIC_SystemReset();
  }
}

bool Watchdog::isResetRequested() const
{
  return _resetRequested;
}

bool Watchdog::isRunning() const
{
  return _running;
}

bool Watchdog::isSupervised() const
{
  return _supervised;
}

uint32_t Watchdog::hardwareTimeoutMs() const
{
  return _hardwareTimeoutMs;
}

uint32_t Watchdog::loopTimeoutMs() const
{
  return _loopTimeoutMs;
}

bool Watchdog::consumeWatchdogResetFlag()
{
  const uint16_t flags = R_SYSTEM->RSTSR1;
  const uint16_t watchdogFlags =
    R_SYSTEM_RSTSR1_WDTRF_Msk | R_SYSTEM_RSTSR1_IWDTRF_Msk;

  // The flags are cleared by writing 0 after they have been read as 1.
  R_SYSTEM->RSTSR1 = static_cast<uint16_t>(flags & ~watchdogFlags);

  return (flags & watchdogFlags) != 0;
}

void Watchdog::timerCallback(timer_callback_args_t *args)
{
  if (args == nullptr || args->p_context == nullptr)
  {
    return;
  }

  Watchdog *self =
    static_cast<Watchdog *>(const_cast<void *>(args->p_context));

  if (self->_resetRequested)
  {
    return;
  }

  const uint32_t ticks = self->_ticksSinceRefresh;

  if (ticks < self->_loopTimeoutTicks)
  {
    self->_ticksSinceRefresh = ticks + 1;
    WDT.refresh();
  }
  // Otherwise stop refreshing: the WDT resets the MCU one period later.
}
