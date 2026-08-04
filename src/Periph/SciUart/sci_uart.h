#ifndef PERIPH_SCIUART_SCI_UART_H
#define PERIPH_SCIUART_SCI_UART_H

#include <stdbool.h>
#include <stdint.h>

#include <bsp_arm_exceptions.h> /* Defines IRQn_Type, required before R7FA4M1AB.h. */
#include <R7FA4M1AB.h>

/**
  * @brief Static description of one RA4M1 SCI channel wired up as an
  *        asynchronous UART.
  * @param peripheral (R_SCI0_Type*) Non-null SCI peripheral base (R_SCI0..R_SCI9).
  * @param mstpBit (uint8_t) Bit index in R_MSTP->MSTPCRB that stops/starts
  *        this channel (31 - SCI channel number).
  * @param portNum (uint8_t) PFS port number carrying the TXD/RXD pins.
  * @param txPinNum (uint8_t) PFS pin number for TXD.
  * @param rxPinNum (uint8_t) PFS pin number for RXD.
  * @param pselGroup (uint8_t) Pin function select value for this channel's
  *        SCI group (0x04 for SCI0/2/4/6/8, 0x05 for SCI1/3/5/7/9).
  */
typedef struct {
    R_SCI0_Type * peripheral;
    uint8_t       mstpBit;
    uint8_t       portNum;
    uint8_t       txPinNum;
    uint8_t       rxPinNum;
    uint8_t       pselGroup;
} SciUart_Instance;

/**
  * @brief Configure the given SCI channel as an 8N1 asynchronous UART.
  * @param instance (const SciUart_Instance*) Non-null channel description.
  * @param baudRate (uint32_t) Requested bit rate, for example 115200.
  * @note Core/Clock's Clock_Init() must run first so PCLKB (24 MHz) is
  *       known; the baud-rate divisor here assumes that value.
  */
void SciUart_Init(const SciUart_Instance * instance, uint32_t baudRate);

/**
  * @brief Transmit one byte. Blocks until the transmit buffer accepts it.
  */
void SciUart_WriteByte(const SciUart_Instance * instance, uint8_t data);

/**
  * @brief Transmit a NUL-terminated string.
  */
void SciUart_WriteString(const SciUart_Instance * instance, const char * text);

/**
  * @brief Non-blocking read of one received byte.
  * @param outData (uint8_t*) Non-null; receives the byte when available.
  * @retval (bool) true if a byte was available and copied to outData.
  */
bool SciUart_TryReadByte(const SciUart_Instance * instance, uint8_t * outData);

#endif /* PERIPH_SCIUART_SCI_UART_H */
