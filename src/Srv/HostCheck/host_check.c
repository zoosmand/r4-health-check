#include "host_check.h"

#include "../../Core/Clock/clock.h"
#include "../../Core/Rtc/rtc.h"
#include "../WifiModem/wifi_modem.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST_CHECK_HOST                "secure.intraclear.com"
#define HOST_CHECK_PORT                (443U)
#define HOST_CHECK_INTERVAL_MS         (60000U)
#define HOST_CHECK_CONNECT_TIMEOUT_MS  (10000U)
#define HOST_CHECK_RESPONSE_TIMEOUT_MS (10000U)
#define HOST_CHECK_RESPONSE_CAP        (96U)

static const char request[] =
    "HEAD / HTTP/1.1\r\n"
    "Host: " HOST_CHECK_HOST "\r\n"
    "User-Agent: r4-health-check/1\r\n"
    "Connection: close\r\n"
    "\r\n";

static HostCheck_Result latestResult;
static uint32_t lastCheckTick;
static bool firstCheckPending;

void HostCheck_Init(void)
{
    memset(&latestResult, 0, sizeof(latestResult));
    lastCheckTick = Clock_GetTickMs();
    firstCheckPending = true;
}

static void hostCheck_Run(void)
{
    char response[HOST_CHECK_RESPONSE_CAP];
    int32_t socket;
    uint32_t responseStart;
    int32_t available;
    int32_t received = -1;

    latestResult.completed = true;
    latestResult.success = false;
    latestResult.httpStatus = 0;
    latestResult.checkedAtEpoch = Rtc_GetEpoch();
    printf("HTTPS check starting\r\n");

    socket = WifiModem_SslClientBegin();
    if (socket < 0) {
        printf("HTTPS check failed: socket allocation\r\n");
        return;
    }

    if (!WifiModem_SslClientUseCaBundle(socket)) {
        printf("HTTPS check failed: CA bundle\r\n");
        WifiModem_SslClientClose(socket);
        return;
    }
    if (!WifiModem_SslClientConnect(socket, HOST_CHECK_HOST, HOST_CHECK_PORT,
                                    HOST_CHECK_CONNECT_TIMEOUT_MS)) {
        printf("HTTPS check failed: TLS connect\r\n");
        WifiModem_SslClientClose(socket);
        return;
    }
    if (!WifiModem_SslClientWrite(socket, request, strlen(request))) {
        printf("HTTPS check failed: request send\r\n");
        WifiModem_SslClientClose(socket);
        return;
    }

    responseStart = Clock_GetTickMs();
    do {
        available = WifiModem_SslClientAvailable(socket);
        if (available > 0) {
            received = WifiModem_SslClientRead(socket, response, sizeof(response));
            break;
        }
    } while ((Clock_GetTickMs() - responseStart) < HOST_CHECK_RESPONSE_TIMEOUT_MS);

    if (received > 0 && strncmp(response, "HTTP/", 5U) == 0) {
        const char * statusStart = strchr(response, ' ');
        if (statusStart != NULL) {
            latestResult.httpStatus = (uint16_t) strtoul(statusStart + 1, NULL, 10);
            latestResult.success = latestResult.httpStatus >= 200U && latestResult.httpStatus < 400U;
        }
    }

    WifiModem_SslClientClose(socket);
    printf("HTTPS check %s, HTTP %u\r\n", latestResult.success ? "passed" : "failed",
           (unsigned int) latestResult.httpStatus);
}

void HostCheck_Poll(void)
{
    uint32_t now = Clock_GetTickMs();

    if (firstCheckPending || (now - lastCheckTick) >= HOST_CHECK_INTERVAL_MS) {
        firstCheckPending = false;
        hostCheck_Run();
        lastCheckTick = Clock_GetTickMs();
    }
}

void HostCheck_GetLatest(HostCheck_Result * resultOut)
{
    if (resultOut != NULL) {
        *resultOut = latestResult;
    }
}
