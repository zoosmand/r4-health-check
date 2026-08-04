#ifndef CORE_RTC_RTC_H
#define CORE_RTC_RTC_H

#include <stdbool.h>
#include <stdint.h>

/** Configure the hardware RTC in binary mode and set Unix epoch seconds. */
bool Rtc_SetEpoch(uint32_t epochSeconds);

/** Read Unix epoch seconds from the hardware RTC. */
uint32_t Rtc_GetEpoch(void);

/** Return the initialization stage that most recently timed out, or zero. */
uint8_t Rtc_GetLastError(void);

#endif /* CORE_RTC_RTC_H */
