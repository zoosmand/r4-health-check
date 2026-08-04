#include "rtc.h"

#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>

#define RTC_REGISTER_WAIT_LIMIT (1000000U)
#define RTC_LOCO_FREQUENCY_COMPARE (255U)

static uint8_t rtcLastError;

static bool rtc_WaitForBit(volatile uint8_t * registerAddress, uint8_t mask, bool set)
{
    uint32_t attempts;

    for (attempts = 0; attempts < RTC_REGISTER_WAIT_LIMIT; attempts++) {
        bool isSet = ((*registerAddress & mask) != 0U);
        if (isSet == set) {
            return true;
        }
    }

    return false;
}

bool Rtc_SetEpoch(uint32_t epochSeconds)
{
    rtcLastError = 0U;
    /* Match the UNO R4 Arduino core: the on-chip LOCO is the supported RTC
       source on this board, so it needs no external crystal startup time. */
    R_RTC->RCR4 = R_RTC_RCR4_RCKSEL_Msk;
    for (volatile uint32_t delay = 0U; delay < 10000U; delay++) {}
    if (!rtc_WaitForBit(&R_RTC->RCR4, R_RTC_RCR4_RCKSEL_Msk, true)) {
        rtcLastError = 1U;
        return false;
    }

    R_RTC->RCR2_b.START = 0;
    if (!rtc_WaitForBit(&R_RTC->RCR2, R_RTC_RCR2_START_Msk, false)) {
        rtcLastError = 2U;
        return false;
    }

    R_RTC->RCR2 = 0;
    if (!rtc_WaitForBit(&R_RTC->RCR2, R_RTC_RCR2_CNTMD_Msk, false)) {
        rtcLastError = 3U;
        return false;
    }

    /* Required LOCO divider value used by Renesas' UNO R4 RTC configuration. */
    R_RTC->RFRH = 0U;
    R_RTC->RFRL = RTC_LOCO_FREQUENCY_COMPARE;

    R_RTC->RCR2_b.CNTMD = 1;
    if (!rtc_WaitForBit(&R_RTC->RCR2, R_RTC_RCR2_CNTMD_Msk, true)) {
        rtcLastError = 4U;
        return false;
    }

    R_RTC->RCR2_b.RESET = 1;
    if (!rtc_WaitForBit(&R_RTC->RCR2, R_RTC_RCR2_RESET_Msk, false)) {
        rtcLastError = 5U;
        return false;
    }

    R_RTC->BCNT0 = (uint8_t) epochSeconds;
    R_RTC->BCNT1 = (uint8_t) (epochSeconds >> 8U);
    R_RTC->BCNT2 = (uint8_t) (epochSeconds >> 16U);
    R_RTC->BCNT3 = (uint8_t) (epochSeconds >> 24U);

    R_RTC->RCR2_b.START = 1;
    if (!rtc_WaitForBit(&R_RTC->RCR2, R_RTC_RCR2_START_Msk, true)) {
        rtcLastError = 6U;
        return false;
    }
    return true;
}

uint8_t Rtc_GetLastError(void)
{
    return rtcLastError;
}

static uint32_t rtc_ReadCounter(void)
{
    uint32_t value;

    value = (uint32_t) R_RTC->BCNT0;
    value |= (uint32_t) R_RTC->BCNT1 << 8U;
    value |= (uint32_t) R_RTC->BCNT2 << 16U;
    value |= (uint32_t) R_RTC->BCNT3 << 24U;
    return value;
}

uint32_t Rtc_GetEpoch(void)
{
    uint32_t first;
    uint32_t second;

    do {
        first = rtc_ReadCounter();
        second = rtc_ReadCounter();
    } while (first != second);

    return second;
}
