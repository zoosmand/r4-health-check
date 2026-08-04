#ifndef CORE_CLOCK_CLOCK_H
#define CORE_CLOCK_CLOCK_H

#include <stdint.h>

/**
  * @brief Start the 48 MHz HOCO, select it as the system clock, configure
  *        the ICLK/PCLK dividers, and start a 1 ms SysTick tick.
  * @note Must run before any peripheral driver that depends on a known
  *       PCLK frequency (for example Periph/WifiUart).
  */
void Clock_Init(void);

/**
  * @brief Busy-wait for at least the given number of milliseconds.
  * @param milliseconds (uint32_t) Delay duration.
  */
void Clock_DelayMs(uint32_t milliseconds);

/**
  * @retval (uint32_t) Milliseconds elapsed since Clock_Init(), wrapping at
  *         UINT32_MAX.
  */
uint32_t Clock_GetTickMs(void);

#endif /* CORE_CLOCK_CLOCK_H */
