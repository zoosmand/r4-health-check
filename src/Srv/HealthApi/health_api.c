#include "health_api.h"

#include "../WifiModem/wifi_modem.h"
#include "../HostCheck/host_check.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HEALTH_API_REQUEST_CAP (160U)

static const char notFoundResponse[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 21\r\n"
    "Connection: close\r\n"
    "\r\n"
    "{\"error\":\"not found\"}";

static int32_t serverSocket = -1;
static int32_t clientSocket = -1;

bool HealthApi_Start(uint16_t port)
{
    serverSocket = WifiModem_ServerBegin(port);
    return serverSocket >= 0;
}

void HealthApi_Poll(void)
{
    char request[HEALTH_API_REQUEST_CAP];
    char healthResponse[256];
    char healthBody[160];
    HostCheck_Result check;
    int32_t available;
    int32_t received;
    const char * response;

    if (serverSocket < 0) {
        return;
    }

    if (clientSocket < 0) {
        clientSocket = WifiModem_ServerAvailable(serverSocket);
        if (clientSocket < 0) {
            return;
        }
    }

    available = WifiModem_ClientAvailable(clientSocket);
    if (available <= 0) {
        return;
    }

    received = WifiModem_ClientRead(clientSocket, request, sizeof(request));
    if (received <= 0) {
        WifiModem_ClientClose(clientSocket);
        clientSocket = -1;
        return;
    }

    if (strncmp(request, "GET /health HTTP/", 17U) == 0) {
        HostCheck_GetLatest(&check);
        snprintf(healthBody, sizeof(healthBody),
                 "{\"status\":\"%s\",\"check\":{\"host\":\"secure.intraclear.com\","
                 "\"completed\":%s,\"success\":%s,\"http_status\":%u,\"checked_at\":%lu}}",
                 check.completed && !check.success ? "degraded" : "ok",
                 check.completed ? "true" : "false",
                 check.success ? "true" : "false",
                 (unsigned int) check.httpStatus, (unsigned long) check.checkedAtEpoch);
        snprintf(healthResponse, sizeof(healthResponse),
                 "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                 "Content-Length: %u\r\nConnection: close\r\n\r\n%s",
                 (unsigned int) strlen(healthBody), healthBody);
        response = healthResponse;
    } else {
        response = notFoundResponse;
    }
    (void) WifiModem_ClientWrite(clientSocket, response, strlen(response));
    WifiModem_ClientClose(clientSocket);
    clientSocket = -1;
}
