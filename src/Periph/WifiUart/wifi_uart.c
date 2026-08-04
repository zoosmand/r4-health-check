#include "wifi_uart.h"

#include "../SciUart/sci_uart.h"

/* D24/D25 on the UNO R4 WiFi variant map to port 5, pins 1 and 2, wired to
   SCI1's TXD1/RXD1 function (Arduino core: pins_arduino.h/variant.cpp,
   pinmux table entries P501/P502). SCI1 is in the SCI1/3/5/7/9 pin function
   group (PSEL 0x05); its module-stop bit is MSTPCRB bit (31 - 1) = 30. */
static const SciUart_Instance wifiUartInstance = {
    .peripheral = R_SCI1,
    .mstpBit    = 30U,
    .portNum    = 5U,
    .txPinNum   = 1U, /* P501 / D24, TXD1 */
    .rxPinNum   = 2U, /* P502 / D25, RXD1 */
    .pselGroup  = 0x05U,
};

void WifiUart_Init(uint32_t baudRate)
{
    SciUart_Init(&wifiUartInstance, baudRate);
}

void WifiUart_WriteByte(uint8_t data)
{
    SciUart_WriteByte(&wifiUartInstance, data);
}

void WifiUart_WriteString(const char * text)
{
    SciUart_WriteString(&wifiUartInstance, text);
}

bool WifiUart_TryReadByte(uint8_t * outData)
{
    return SciUart_TryReadByte(&wifiUartInstance, outData);
}
