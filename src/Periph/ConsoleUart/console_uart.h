#ifndef PERIPH_CONSOLEUART_CONSOLE_UART_H
#define PERIPH_CONSOLEUART_CONSOLE_UART_H

#include <stdbool.h>
#include <stdint.h>

/**
  * @brief Configure SCI9 (TXD9 on D22, RXD9 on D23) as an asynchronous 8N1
  *        UART, intended for a USB-serial adapter or a debug probe's UART
  *        passthrough.
  * @param baudRate (uint32_t) Requested bit rate, for example 115200.
  * @note Core/Clock's Clock_Init() must run first so PCLKB is known; the
  *       baud-rate divisor here assumes the 24 MHz PCLKB it configures.
  */
void ConsoleUart_Init(uint32_t baudRate);

/**
  * @brief Transmit one byte. Blocks until the transmit buffer accepts it.
  * @param data (uint8_t) Byte to send.
  */
void ConsoleUart_WriteByte(uint8_t data);

/**
  * @brief Transmit a NUL-terminated string.
  * @param text (const char*) Non-null string to send.
  */
void ConsoleUart_WriteString(const char * text);

/**
  * @brief Non-blocking read of one received byte.
  * @param outData (uint8_t*) Non-null; receives the byte when available.
  * @retval (bool) true if a byte was available and copied to outData.
  */
bool ConsoleUart_TryReadByte(uint8_t * outData);

#endif /* PERIPH_CONSOLEUART_CONSOLE_UART_H */
