#include "console_uart.h"

#include "../SciUart/sci_uart.h"

/* D0/D1 on the UNO R4 WiFi variant map to port 3, pins 1 and 2, wired to
   SCI2's RXD2/TXD2 function (Arduino core: pins_arduino.h/variant.cpp,
   pinmux table entries P301/P302 - this is the same channel Arduino calls
   Serial1). These are the only console-capable pins broken out to the
   board's main header: the SCI9 alternative (Arduino pin indices 22/23,
   P109/P110) is a pair of bare debug test pads next to the ESP32 module,
   not accessible without soldering. SCI2 is in the SCI0/2/4/6/8 pin
   function group (PSEL 0x04); its module-stop bit is MSTPCRB bit
   (31 - 2) = 29. */
static const SciUart_Instance consoleUartInstance = {
    .peripheral = R_SCI2,
    .mstpBit    = 29U,
    .portNum    = 3U,
    .txPinNum   = 2U, /* P302 / D1, TXD2 */
    .rxPinNum   = 1U, /* P301 / D0, RXD2 */
    .pselGroup  = 0x04U,
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
