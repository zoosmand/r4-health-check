#ifndef SRV_WIFIMODEM_WIFI_MODEM_H
#define SRV_WIFIMODEM_WIFI_MODEM_H

#include <stdbool.h>
#include <stdint.h>

/**
  * @brief Bring up the UART link to the onboard ESP32-S3 WiFi co-processor
  *        and reset its WiFi state machine.
  * @retval (bool) true once the co-processor answered OK to a soft reset.
  * @note Blocks for up to a few seconds while the co-processor boots.
  */
bool WifiModem_Init(void);

/**
  * @brief Join a WPA/WPA2 (or open) access point in station mode and wait
  *        for the connection to complete.
  * @param ssid (const char*) Non-null network name, at most 32 characters.
  * @param passphrase (const char*) Passphrase, or NULL/"" for an open
  *        network.
  * @param timeoutMs (uint32_t) Maximum time to wait for the connected state.
  * @retval (bool) true if the co-processor reports the connected state
  *         before the timeout elapses.
  */
bool WifiModem_Connect(const char * ssid, const char * passphrase, uint32_t timeoutMs);

#endif /* SRV_WIFIMODEM_WIFI_MODEM_H */
