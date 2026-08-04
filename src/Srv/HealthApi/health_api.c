#include "health_api.h"

#include "../WifiModem/wifi_modem.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HEALTH_API_REQUEST_CAP (160U)

static const char healthResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 15\r\n"
    "Connection: close\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";

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

    response = (strncmp(request, "GET /health HTTP/", 17U) == 0) ? healthResponse : notFoundResponse;
    (void) WifiModem_ClientWrite(clientSocket, response, strlen(response));
    WifiModem_ClientClose(clientSocket);
    clientSocket = -1;
}
