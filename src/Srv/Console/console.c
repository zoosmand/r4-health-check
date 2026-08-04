#include "console.h"

#include "../../Periph/ConsoleUart/console_uart.h"

#include <cmsis_compiler.h>
#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>

#include <stdio.h>
#include <string.h>

#define CONSOLE_LINE_BUFFER_CAP   (32U)

/* PRCR unlock key. PRC1 gates the VBTBKR backup registers used below. */
#define CONSOLE_PRCR_KEY  (0xA500U)
#define CONSOLE_PRCR_PRC1 (1U << 1)

/* Convention used by this board's factory (Arduino) bootloader: a magic
   value left in the VBATT backup register survives a software reset, and
   the resident bootloader checks it on the next boot to decide whether to
   stay resident instead of jumping straight to the application (the same
   mechanism behind the factory firmware's "double-tap reset"). */
#define CONSOLE_BOOT_MAGIC_REGISTER (*((volatile uint32_t *) &R_SYSTEM->VBTBKR[0]))
#define CONSOLE_BOOT_MAGIC_VALUE    (0x07738135UL)

static char lineBuffer[CONSOLE_LINE_BUFFER_CAP];
static uint32_t lineLength = 0;

void Console_Init(uint32_t baudRate)
{
    ConsoleUart_Init(baudRate);
    (void) setvbuf(stdout, NULL, _IONBF, 0);
    (void) setvbuf(stderr, NULL, _IONBF, 0);
}

/* Newlib calls _write() for stdout/stderr. Sending the supplied byte count,
   rather than treating the data as a C string, also preserves embedded NULs. */
int _write(int file, char * data, int length)
{
    int index;

    (void) file;

    for (index = 0; index < length; index++) {
        ConsoleUart_WriteByte((uint8_t) data[index]);
    }

    return length;
}

static void console_EnterBootloader(void)
{
    printf("Entering bootloader...\r\n");

    R_SYSTEM->PRCR = (uint16_t) (CONSOLE_PRCR_KEY | CONSOLE_PRCR_PRC1);
    CONSOLE_BOOT_MAGIC_REGISTER = CONSOLE_BOOT_MAGIC_VALUE;
    R_SYSTEM->PRCR = (uint16_t) CONSOLE_PRCR_KEY;

    NVIC_SystemReset();
    while (1) {} /* Unreachable: NVIC_SystemReset() does not return. */
}

static void console_DispatchLine(const char * line)
{
    if (strcmp(line, "boot") == 0) {
        console_EnterBootloader();
    } else if (line[0] != '\0') {
        printf("ERR unknown command: %s\r\n", line);
    }
}

void Console_Poll(void)
{
    uint8_t receivedByte;

    while (ConsoleUart_TryReadByte(&receivedByte)) {
        ConsoleUart_WriteByte(receivedByte); /* Local echo so typed input is visible. */

        if (receivedByte == '\r' || receivedByte == '\n') {
            if (lineLength > 0U) {
                lineBuffer[lineLength] = '\0';
                console_DispatchLine(lineBuffer);
                lineLength = 0;
            }
        } else if (lineLength < (CONSOLE_LINE_BUFFER_CAP - 1U)) {
            lineBuffer[lineLength] = (char) receivedByte;
            lineLength++;
        }
    }
}
