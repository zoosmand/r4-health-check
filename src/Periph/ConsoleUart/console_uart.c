#include "console_uart.h"

#include "../SciUart/sci_uart.h"

/* D22/D23 on the UNO R4 WiFi variant map to port 1, pins 9 and 10, wired to
   SCI9's TXD9/RXD9 function (Arduino core: pins_arduino.h/variant.cpp,
   pinmux table entries P109/P110). SCI9 is in the SCI1/3/5/7/9 pin function
   group (PSEL 0x05); its module-stop bit is MSTPCRB bit (31 - 9) = 22. */
static const SciUart_Instance consoleUartInstance = {
    .peripheral = R_SCI9,
    .mstpBit    = 22U,
    .portNum    = 1U,
    .txPinNum   = 9U,  /* P109 / D22, TXD9 */
    .rxPinNum   = 10U, /* P110 / D23, RXD9 */
    .pselGroup  = 0x05U,
};

void ConsoleUart_Init(uint32_t baudRate)
{
    SciUart_Init(&consoleUartInstance, baudRate);
}

void ConsoleUart_WriteByte(uint8_t data)
{
    SciUart_WriteByte(&consoleUartInstance, data);
}

void ConsoleUart_WriteString(const char * text)
{
    SciUart_WriteString(&consoleUartInstance, text);
}

bool ConsoleUart_TryReadByte(uint8_t * outData)
{
    return SciUart_TryReadByte(&consoleUartInstance, outData);
}
