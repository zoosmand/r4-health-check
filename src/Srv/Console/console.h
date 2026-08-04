#ifndef SRV_CONSOLE_CONSOLE_H
#define SRV_CONSOLE_CONSOLE_H

#include <stdint.h>

/**
  * @brief Configure the debug console UART (D22/D23, SCI9).
  * @param baudRate (uint32_t) Requested bit rate, for example 115200.
  */
void Console_Init(uint32_t baudRate);

/**
  * @brief Poll for and process console input. Never blocks; call this
  *        regularly (for example every main-loop iteration) so typed
  *        commands are handled promptly.
  * @note Recognized commands:
  *       - "boot" resets into the board's resident bootloader, using the
  *         same VBATT-backup-register signal the factory firmware's
  *         double-tap reset uses.
  */
void Console_Poll(void);

#endif /* SRV_CONSOLE_CONSOLE_H */
