#ifndef SRV_WIFIMODEM_WIFI_MODEM_H
#define SRV_WIFIMODEM_WIFI_MODEM_H

#include <stdbool.h>
#include <stddef.h>
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

/** Query the station IPv4 address after a successful connection. */
bool WifiModem_GetLocalIp(char * ipOut, size_t ipCap);

/** Open a TCP listening socket. The returned socket is negative on failure. */
int32_t WifiModem_ServerBegin(uint16_t port);

/** Accept or retrieve a pending client; returns a negative value if none exists. */
int32_t WifiModem_ServerAvailable(int32_t serverSocket);

/** Return the number of bytes waiting for a connected client. */
int32_t WifiModem_ClientAvailable(int32_t clientSocket);

/** Read up to dataCap bytes from a client; returns a negative value on error. */
int32_t WifiModem_ClientRead(int32_t clientSocket, char * dataOut, size_t dataCap);

/** Send an exact byte sequence to a connected client. */
bool WifiModem_ClientWrite(int32_t clientSocket, const char * data, size_t dataLength);

/** Close a client socket. */
void WifiModem_ClientClose(int32_t clientSocket);

#endif /* SRV_WIFIMODEM_WIFI_MODEM_H */
