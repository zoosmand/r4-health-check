#include "clock.h"

#include <cmsis_compiler.h>
#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>

/* HOCO on the UNO R4 WiFi is factory-trimmed to 48 MHz (OFS1.HOCOFRQ0 option
   setting memory); the RA4M1 exposes no runtime HOCO frequency-select
   register, so this frequency is fixed for this board. */
#define CLOCK_HOCO_HZ (48000000U)

/* PRCR unlock key. PRC0 gates the clock-generation registers written below
   (HOCOCR, SCKSCR, SCKDIVCR, MEMWAIT). */
#define CLOCK_PRCR_KEY  (0xA500U)
#define CLOCK_PRCR_PRC0 (1U << 0)

static volatile uint32_t tickMs = 0;

void SysTick_Handler(void)
{
    tickMs++;
}

void Clock_Init(void)
{
    R_SYSTEM->PRCR = (uint16_t) (CLOCK_PRCR_KEY | CLOCK_PRCR_PRC0);

    /* Start HOCO and wait for the oscillator to stabilize. */
    R_SYSTEM->HOCOCR_b.HCSTP = 0;
    while (R_SYSTEM->OSCSF_b.HOCOSF == 0) {}

    /* The hardware manual prohibits ICLK > 32 MHz while MEMWAIT is 0: flash
       can't supply instructions fast enough without an extra wait cycle.
       This must be set before switching ICLK to 48 MHz below, or the CPU
       faults on its own fetches almost immediately. */
    R_SYSTEM->MEMWAIT_b.MEMWAIT = 1;

    /* ICLK /1 (48 MHz), PCLKB /2 (24 MHz). PCLKB clocks the SCI channels
       used by Periph/WifiUart, so its value must stay in sync with the
       baud-rate math there. */
    R_SYSTEM->SCKDIVCR_b.ICK  = 0;
    R_SYSTEM->SCKDIVCR_b.PCKA = 0;
    R_SYSTEM->SCKDIVCR_b.PCKB = 1;
    R_SYSTEM->SCKDIVCR_b.PCKC = 0;
    R_SYSTEM->SCKDIVCR_b.PCKD = 0;
    R_SYSTEM->SCKDIVCR_b.FCK  = 1;

    R_SYSTEM->SCKSCR_b.CKSEL = 0; /* 0 = HOCO */

    R_SYSTEM->PRCR = (uint16_t) CLOCK_PRCR_KEY;

    SystemCoreClock = CLOCK_HOCO_HZ;
    (void) SysTick_Config(SystemCoreClock / 1000U);

    /* The resident bootloader hands off to the application with a direct
       jump, not a full CPU reset, so PRIMASK can still be set from whatever
       the bootloader left it as. Without this, SysTick (and every other
       interrupt) counts down correctly but is never actually taken. */
    __enable_irq();
}

void Clock_DelayMs(uint32_t milliseconds)
{
    uint32_t start = tickMs;
    while ((tickMs - start) < milliseconds) {}
}

uint32_t Clock_GetTickMs(void)
{
    return tickMs;
}
