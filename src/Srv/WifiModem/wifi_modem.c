#include "wifi_modem.h"

#include "../../Core/Clock/clock.h"
#include "../../Periph/WifiUart/wifi_uart.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* AT-style command set implemented by the ESP32-S3 co-processor firmware
   (Arduino WiFiS3 library: WiFiCommands.h / WiFi.cpp), reused here since it
   is the protocol the co-processor actually speaks; nothing about it is
   Arduino-framework-specific. */
#define WIFI_MODEM_BAUD_RATE        (115200U)
#define WIFI_MODEM_RESPONSE_TIMEOUT (2000U)
#define WIFI_MODEM_RESET_RETRIES    (3U)
#define WIFI_MODEM_BUFFER_CAP       (96U)
#define WIFI_MODEM_STATUS_CONNECTED "3" /* wl_status_t WL_CONNECTED */

static bool wifiModem_EndsWith(const char * buffer, uint32_t length, const char * suffix)
{
    uint32_t suffixLength = (uint32_t) strlen(suffix);

    if (length < suffixLength) {
        return false;
    }

    return strncmp(&buffer[length - suffixLength], suffix, suffixLength) == 0;
}

static void wifiModem_DrainRxBuffer(void)
{
    uint8_t discard;

    while (WifiUart_TryReadByte(&discard)) {}
}

/**
  * @brief Send one AT command and wait for its terminating status line.
  * @param command (const char*) Non-null, complete command including the
  *        trailing "\r\n".
  * @param payloadOut (char*) Optional buffer that receives the text after
  *        the first ':' in the response (used for value queries such as
  *        AT+GETSTATUS?); pass NULL if the payload is not needed.
  * @param payloadCap (size_t) Capacity of payloadOut, including the NUL.
  * @param timeoutMs (uint32_t) Maximum time to wait for "OK" or "ERROR".
  * @retval (bool) true if the co-processor answered "OK".
  */
static bool wifiModem_SendCommand(const char * command, char * payloadOut, size_t payloadCap, uint32_t timeoutMs)
{
    char buffer[WIFI_MODEM_BUFFER_CAP];
    uint32_t length = 0;
    bool ok = false;
    bool done = false;
    uint32_t startTick;

    wifiModem_DrainRxBuffer();
    WifiUart_WriteString(command);

    startTick = Clock_GetTickMs();
    while (!done && (Clock_GetTickMs() - startTick) < timeoutMs) {
        uint8_t receivedByte;

        if (!WifiUart_TryReadByte(&receivedByte)) {
            continue;
        }

        if (length < (WIFI_MODEM_BUFFER_CAP - 1U)) {
            buffer[length] = (char) receivedByte;
            length++;
            buffer[length] = '\0';
        }

        if (wifiModem_EndsWith(buffer, length, "OK\r\n")) {
            ok = true;
            done = true;
        } else if (wifiModem_EndsWith(buffer, length, "ERROR\r\n")) {
            ok = false;
            done = true;
        }
    }

    if (ok && payloadOut != NULL && payloadCap > 0U) {
        const char * colon = strchr(buffer, ':');

        payloadOut[0] = '\0';
        if (colon != NULL) {
            const char * valueStart = colon + 1;
            const char * lineEnd = strchr(valueStart, '\r');
            size_t valueLength = (lineEnd != NULL) ? (size_t) (lineEnd - valueStart) : strlen(valueStart);

            if (valueLength >= payloadCap) {
                valueLength = payloadCap - 1U;
            }
            memcpy(payloadOut, valueStart, valueLength);
            payloadOut[valueLength] = '\0';
        }
    }

    return ok;
}

bool WifiModem_Init(void)
{
    uint32_t attempt;

    WifiUart_Init(WIFI_MODEM_BAUD_RATE);

    for (attempt = 0; attempt < WIFI_MODEM_RESET_RETRIES; attempt++) {
        if (wifiModem_SendCommand("AT+SOFTRESETWIFI\r\n", NULL, 0U, WIFI_MODEM_RESPONSE_TIMEOUT)) {
            return true;
        }
    }

    return false;
}

bool WifiModem_Connect(const char * ssid, const char * passphrase, uint32_t timeoutMs)
{
    char command[80];
    char statusPayload[8];
    uint32_t startTick;

    if (!wifiModem_SendCommand("AT+WIFIMODE=1\r\n", NULL, 0U, WIFI_MODEM_RESPONSE_TIMEOUT)) {
        return false;
    }

    if (passphrase != NULL && passphrase[0] != '\0') {
        snprintf(command, sizeof(command), "AT+BEGINSTA=%s,%s\r\n", ssid, passphrase);
    } else {
        snprintf(command, sizeof(command), "AT+BEGINSTA=%s\r\n", ssid);
    }

    if (!wifiModem_SendCommand(command, NULL, 0U, WIFI_MODEM_RESPONSE_TIMEOUT)) {
        return false;
    }

    startTick = Clock_GetTickMs();
    while ((Clock_GetTickMs() - startTick) < timeoutMs) {
        if (wifiModem_SendCommand("AT+GETSTATUS?\r\n", statusPayload, sizeof(statusPayload), WIFI_MODEM_RESPONSE_TIMEOUT)) {
            if (strcmp(statusPayload, WIFI_MODEM_STATUS_CONNECTED) == 0) {
                return true;
            }
        }
        Clock_DelayMs(250);
    }

    return false;
}
