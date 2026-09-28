#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <Arduino.h>
#include <FspTimer.h>

/*
  Supervised hardware watchdog.

  The RA4M1 WDT period is at most ~5.6 s, but a single Wi-Fi module call can
  legitimately block for up to 10 s (the WiFiS3 modem timeout). Refreshing
  the WDT only from the main loop would therefore reset the board whenever a
  monitored server is slow.

  Instead, a periodic timer interrupt refreshes the WDT as long as the main
  loop has called refresh() within the loop timeout. When the main loop stops
  calling refresh() for longer than that, the interrupt stops refreshing and
  the WDT resets the MCU one hardware period later. If the interrupt itself
  stops, the WDT resets the MCU after one hardware period.

  If no timer is available, refresh() refreshes the WDT directly, and the
  effective loop timeout is the hardware period.

  requestReset() stops all refreshing, so a deliberate restart is also
  performed by the WDT and is reported by consumeWatchdogResetFlag().
*/
class Watchdog
{
public:
  Watchdog();

  /**
    * @brief Start the hardware watchdog and its supervising interrupt.
    *        Once started, the watchdog cannot be stopped.
    * @param hardwareTimeoutMs (uint32_t) Requested WDT period; rounded up to
    *        the next supported value, at most ~5592 ms.
    * @param loopTimeoutMs (uint32_t) Longest allowed gap between refresh()
    *        calls from the main loop.
    * @retval (bool) True when the WDT is running.
    */
  bool begin(uint32_t hardwareTimeoutMs, uint32_t loopTimeoutMs);

  /**
    * @brief Report that the main loop is alive. Cheap; safe to call often
    *        and before begin(). Call from thread context only.
    */
  void refresh();

  /**
    * @brief Stop refreshing the WDT so that it resets the MCU within one
    *        hardware period (~5.6 s). Irreversible; later refresh() calls are
    *        ignored. When the WDT is not running, resets the MCU immediately
    *        with NVIC_SystemReset(). Call from thread context only.
    */
  void requestReset();

  bool isResetRequested() const;

  bool isRunning() const;

  /**
    * @brief Report whether the supervising interrupt is active.
    * @retval (bool) False when refresh() feeds the WDT directly.
    */
  bool isSupervised() const;

  /**
    * @brief Effective hardware WDT period.
    * @retval (uint32_t) Milliseconds, or 0 before begin().
    */
  uint32_t hardwareTimeoutMs() const;

  /**
    * @brief Effective main-loop timeout.
    * @retval (uint32_t) Milliseconds, or 0 before begin().
    */
  uint32_t loopTimeoutMs() const;

  /**
    * @brief Read and clear the watchdog reset flags. Call once, early in
    *        setup(). Best effort: reports false if the bootloader cleared
    *        the flags.
    * @retval (bool) True when the last reset was caused by the WDT or IWDT.
    */
  static bool consumeWatchdogResetFlag();

private:
  static void timerCallback(timer_callback_args_t *args);

  FspTimer _timer;
  bool _running;
  bool _supervised;
  uint32_t _hardwareTimeoutMs;
  uint32_t _loopTimeoutMs;
  uint32_t _loopTimeoutTicks;

  // Reset to 0 by refresh() with a single store; incremented by the
  // interrupt, which cannot be preempted by the main loop.
  volatile uint32_t _ticksSinceRefresh;

  // Set once by requestReset(); read by the interrupt.
  volatile bool _resetRequested;
};

#endif
