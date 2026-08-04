#ifndef PERIPH_WIFIUART_WIFI_UART_H
#define PERIPH_WIFIUART_WIFI_UART_H

#include <stdbool.h>
#include <stdint.h>

/**
  * @brief Configure SCI1 (TXD1 on D24, RXD1 on D25) as an asynchronous 8N1
  *        UART. On the UNO R4 WiFi this link connects to the onboard
  *        ESP32-S3 WiFi/BLE co-processor.
  * @param baudRate (uint32_t) Requested bit rate, for example 115200.
  * @note Core/Clock's Clock_Init() must run first so PCLKB is known; the
  *       baud-rate divisor here assumes the 24 MHz PCLKB it configures.
  */
void WifiUart_Init(uint32_t baudRate);

/**
  * @brief Transmit one byte. Blocks until the transmit buffer accepts it.
  * @param data (uint8_t) Byte to send.
  */
void WifiUart_WriteByte(uint8_t data);

/**
  * @brief Transmit a NUL-terminated string.
  * @param text (const char*) Non-null string to send.
  */
void WifiUart_WriteString(const char * text);

/**
  * @brief Non-blocking read of one received byte.
  * @param outData (uint8_t*) Non-null; receives the byte when available.
  * @retval (bool) true if a byte was available and copied to outData.
  */
bool WifiUart_TryReadByte(uint8_t * outData);

#endif /* PERIPH_WIFIUART_WIFI_UART_H */
