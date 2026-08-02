#include <cmsis_compiler.h>
#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>

// Onboard "L" LED on the Uno R4 WiFi is wired to pin P1.02.
#define LED_PORT_NUM 1
#define LED_PIN_NUM 2

int main(void)
{
    // Unlock the PFS registers: clear BOWI, then set PFSWE.
    R_PMISC->PWPR = 0;
    R_PMISC->PWPR = 1U << 6;

    R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PDR = 1;  // Output
    R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PMR = 0;  // General I/O, not peripheral

    while (1)
    {
        R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PODR = 1;
        for (volatile int i = 0; i < 8 * 1000 * 2000 / 2; i++) {}

        R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PODR = 0;
        for (volatile int i = 0; i < 8 * 1000 * 2000 / 2; i++) {}
    }

    return 0;
}

void NMI_Handler(void)
{
    while (1) {}
}
