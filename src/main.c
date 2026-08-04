#include <cmsis_compiler.h>
#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>
#include <stdbool.h>

#include "Core/Clock/clock.h"
#include "Srv/Console/console.h"
#include "Srv/WifiModem/wifi_credentials.h"
#include "Srv/WifiModem/wifi_modem.h"

// Onboard "L" LED on the Uno R4 WiFi is wired to pin P1.02.
#define LED_PORT_NUM 1
#define LED_PIN_NUM 2

#define CONSOLE_BAUD_RATE       (115200U)
#define WIFI_CONNECT_TIMEOUT_MS (20000U)
#define LED_BLINK_HALF_PERIOD_MS (1000U)

/* Delay while still handling console input, so typed commands (e.g. "boot")
   are not stuck behind a busy-wait. */
static void delayWithConsolePolling(uint32_t milliseconds)
{
    uint32_t start = Clock_GetTickMs();

    while ((Clock_GetTickMs() - start) < milliseconds) {
        Console_Poll();
    }
}

int main(void)
{
    bool wifiConnected;

    Clock_Init();
    Console_Init(CONSOLE_BAUD_RATE);
    Console_Printf("r4-health-check booting...\r\n");

    // Unlock the PFS registers: clear BOWI, then set PFSWE.
    R_PMISC->PWPR = 0;
    R_PMISC->PWPR = 1U << 6;

    R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PDR = 1;  // Output
    R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PMR = 0;  // General I/O, not peripheral

    // TODO: surface connection status via a health service once one
    // exists, instead of just printing it here.
    wifiConnected = WifiModem_Init() && WifiModem_Connect(WIFI_SSID, WIFI_PASSWORD, WIFI_CONNECT_TIMEOUT_MS);
    Console_Printf("WiFi %s\r\n", wifiConnected ? "connected" : "not connected");

    while (1)
    {
        R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PODR = 1;
        delayWithConsolePolling(LED_BLINK_HALF_PERIOD_MS);

        R_PFS->PORT[LED_PORT_NUM].PIN[LED_PIN_NUM].PmnPFS_b.PODR = 0;
        delayWithConsolePolling(LED_BLINK_HALF_PERIOD_MS);
    }

    return 0;
}

void NMI_Handler(void)
{
    while (1) {}
}
