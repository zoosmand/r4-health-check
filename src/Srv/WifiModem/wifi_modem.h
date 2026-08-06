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

/** Obtain current Unix time from the network-connected modem. */
bool WifiModem_GetNetworkTime(uint32_t * epochOut);

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

/** Allocate a TLS client socket. */
int32_t WifiModem_SslClientBegin(void);

/** Select the bridge firmware's built-in CA bundle for a TLS socket. */
bool WifiModem_SslClientUseCaBundle(int32_t clientSocket);

/** Connect a TLS socket to a host name and port. */
bool WifiModem_SslClientConnect(int32_t clientSocket, const char * host, uint16_t port,
                                uint32_t timeoutMs);

/** Return the number of encrypted-response bytes waiting for a TLS client. */
int32_t WifiModem_SslClientAvailable(int32_t clientSocket);

/** Read response bytes from a TLS client. */
int32_t WifiModem_SslClientRead(int32_t clientSocket, char * dataOut, size_t dataCap);

/** Send an exact byte sequence through a TLS client. */
bool WifiModem_SslClientWrite(int32_t clientSocket, const char * data, size_t dataLength);

/** Close a TLS client socket. */
void WifiModem_SslClientClose(int32_t clientSocket);

#endif /* SRV_WIFIMODEM_WIFI_MODEM_H */
