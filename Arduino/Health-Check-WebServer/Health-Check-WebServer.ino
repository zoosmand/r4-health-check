/*
  Arduino UNO R4 WiFi Health Checker

  Functions:
    - Checks an HTTPS endpoint every 60 seconds using HTTP HEAD.
    - HTTP 200 means healthy.
    - Any other HTTP result or connection error activates the buzzer.
    - Provides a small HTTP JSON API on port 80.

  API:
    GET  /api/status
    POST /api/check
    POST /api/buzzer/test
    POST /api/buzzer/silence
    POST /api/buzzer/unsilence
*/

#include "WiFiS3.h"
#include "arduino_secrets.h"

// -----------------------------------------------------------------------------
// Wi-Fi configuration
// -----------------------------------------------------------------------------

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int wifiStatus = WL_IDLE_STATUS;

WiFiServer apiServer(80);

// -----------------------------------------------------------------------------
// Health-check target
// -----------------------------------------------------------------------------

// Do not include "https://" here.
const char HEALTH_HOST[] = URI_HEALTH_HOST;

// Must begin with "/".
const char HEALTH_PATH[] = URI_HEALTH_PATH;

// HTTPS normally uses TCP port 443.
constexpr uint16_t HEALTH_PORT = URI_HEALTH_PORT;

// Check once per minute.
constexpr unsigned long CHECK_INTERVAL_MS = 60UL * 1000UL;

// Maximum time spent waiting for an HTTP response.
constexpr unsigned long HTTP_TIMEOUT_MS = 10000UL;

// -----------------------------------------------------------------------------
// Buzzer configuration
// -----------------------------------------------------------------------------

// Assumption: active buzzer connected between this pin and GND.
constexpr uint8_t BUZZER_PIN = 8;

// Change these when using a module with inverted logic.
constexpr uint8_t BUZZER_ON = HIGH;
constexpr uint8_t BUZZER_OFF = LOW;

// Manual buzzer test duration.
constexpr unsigned long BUZZER_TEST_DURATION_MS = 3000UL;

// -----------------------------------------------------------------------------
// Runtime state
// -----------------------------------------------------------------------------

bool serviceHealthy = false;
bool alarmActive = false;
bool buzzerSilenced = false;
bool firstCheckCompleted = false;
bool checkInProgress = false;

int lastHttpStatus = 0;

unsigned long lastCheckStartedAt = 0;
unsigned long lastCheckCompletedAt = 0;
unsigned long lastCheckDurationMs = 0;

unsigned long successfulChecks = 0;
unsigned long failedChecks = 0;
unsigned long consecutiveFailures = 0;

unsigned long buzzerTestStartedAt = 0;
bool buzzerTestActive = false;

unsigned long lastWiFiReconnectAttempt = 0;

String lastError = "No health check has been performed";

// -----------------------------------------------------------------------------
// Function declarations
// -----------------------------------------------------------------------------

void connectWiFi();
void maintainWiFi();
void printWiFiStatus();

bool performHealthCheck();

void updateBuzzer();
void setBuzzer(bool enabled);

void handleApiClient();
void sendStatusResponse(WiFiClient &client);
void sendJsonResponse(
  WiFiClient &client,
  int statusCode,
  const char *statusText,
  const String &json
);
void sendNotFound(WiFiClient &client);
void sendMethodNotAllowed(WiFiClient &client);

String escapeJson(const String &value);
String boolToJson(bool value);

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup()
{
  pinMode(BUZZER_PIN, OUTPUT);
  setBuzzer(false);

  Serial.begin(115200);

  // Do not wait forever for Serial because the device must work autonomously.
  unsigned long serialWaitStartedAt = millis();

  while (!Serial && millis() - serialWaitStartedAt < 3000UL)
  {
    // Wait briefly for the native USB serial port.
  }

  Serial.println();
  Serial.println("UNO R4 WiFi Health Checker");
  Serial.println("==========================");

  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Communication with the WiFi module failed.");

    // Hardware fault: continuously sound the buzzer.
    while (true)
    {
      setBuzzer(true);
      delay(250);

      setBuzzer(false);
      delay(250);
    }
  }

  String firmwareVersion = WiFi.firmwareVersion();

  Serial.print("WiFi firmware: ");
  Serial.println(firmwareVersion);

  if (firmwareVersion < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("WARNING: WiFi firmware should be upgraded.");
  }

  connectWiFi();

  apiServer.begin();

  Serial.println("API server started on TCP port 80.");

  // Perform the first check immediately after startup.
  performHealthCheck();
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop()
{
  maintainWiFi();
  handleApiClient();
  updateBuzzer();

  const unsigned long now = millis();

  if (!checkInProgress &&
      now - lastCheckStartedAt >= CHECK_INTERVAL_MS)
  {
    performHealthCheck();
  }
}

// -----------------------------------------------------------------------------
// Wi-Fi
// -----------------------------------------------------------------------------

void connectWiFi()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);

    wifiStatus = WiFi.begin(ssid, pass);

    if (wifiStatus == WL_CONNECTED)
    {
      break;
    }

    Serial.println("WiFi connection failed. Retrying in 10 seconds.");

    // Keep the buzzer active while initial Wi-Fi connection is unavailable.
    setBuzzer(true);
    delay(500);
    setBuzzer(false);

    delay(9500);
  }

  printWiFiStatus();
}

void maintainWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  const unsigned long now = millis();

  if (now - lastWiFiReconnectAttempt < 10000UL)
  {
    return;
  }

  lastWiFiReconnectAttempt = now;

  Serial.println("WiFi disconnected. Attempting reconnection.");

  WiFi.disconnect();
  wifiStatus = WiFi.begin(ssid, pass);

  if (wifiStatus == WL_CONNECTED)
  {
    Serial.println("WiFi reconnected.");
    printWiFiStatus();
  }
  else
  {
    Serial.println("WiFi reconnection failed.");
  }
}

void printWiFiStatus()
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();

  Serial.print("IP address: ");
  Serial.println(ip);

  Serial.print("API status URL: http://");
  Serial.print(ip);
  Serial.println("/api/status");

  long rssi = WiFi.RSSI();

  Serial.print("Signal strength: ");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// -----------------------------------------------------------------------------
// HTTPS health check
// -----------------------------------------------------------------------------

bool performHealthCheck()
{
  if (checkInProgress)
  {
    return false;
  }

  checkInProgress = true;
  lastCheckStartedAt = millis();
  lastHttpStatus = 0;
  lastError = "";

  Serial.println();
  Serial.print("Checking https://");
  Serial.print(HEALTH_HOST);
  Serial.println(HEALTH_PATH);

  bool checkSucceeded = false;

  if (WiFi.status() != WL_CONNECTED)
  {
    lastError = "WiFi is disconnected";
  }
  else
  {
    WiFiSSLClient httpsClient;

    httpsClient.setTimeout(HTTP_TIMEOUT_MS);

    Serial.print("Connecting to ");
    Serial.print(HEALTH_HOST);
    Serial.print(":");
    Serial.println(HEALTH_PORT);

    if (!httpsClient.connect(HEALTH_HOST, HEALTH_PORT))
    {
      lastError = "HTTPS connection failed";
    }
    else
    {
      Serial.println("HTTPS connection established.");

      // Send HTTP HEAD request.
      httpsClient.print("HEAD ");
      httpsClient.print(HEALTH_PATH);
      httpsClient.println(" HTTP/1.1");

      httpsClient.print("Host: ");
      httpsClient.println(HEALTH_HOST);

      httpsClient.println("User-Agent: UNO-R4-Health-Checker/1.0");
      httpsClient.println("Accept: */*");
      httpsClient.println("Connection: close");
      httpsClient.println();

      unsigned long responseWaitStartedAt = millis();

      while (!httpsClient.available() &&
             httpsClient.connected() &&
             millis() - responseWaitStartedAt < HTTP_TIMEOUT_MS)
      {
        // Keep the buzzer pattern operational while waiting.
        updateBuzzer();
        delay(1);
      }

      if (!httpsClient.available())
      {
        lastError = "HTTP response timeout";
      }
      else
      {
        String statusLine = httpsClient.readStringUntil('\n');
        statusLine.trim();

        Serial.print("Response: ");
        Serial.println(statusLine);

        // Expected example:
        // HTTP/1.1 200 OK

        if (!statusLine.startsWith("HTTP/"))
        {
          lastError = "Invalid HTTP response";
        }
        else
        {
          int firstSpace = statusLine.indexOf(' ');

          if (firstSpace < 0 || statusLine.length() < firstSpace + 4)
          {
            lastError = "Cannot parse HTTP status";
          }
          else
          {
            lastHttpStatus =
              statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();

            checkSucceeded = lastHttpStatus == 200;

            if (!checkSucceeded)
            {
              lastError = "Unexpected HTTP status";
            }
          }
        }

        // Read and discard the remaining response headers.
        while (httpsClient.connected() || httpsClient.available())
        {
          if (!httpsClient.available())
          {
            break;
          }

          String headerLine = httpsClient.readStringUntil('\n');
          headerLine.trim();

          if (headerLine.length() == 0)
          {
            break;
          }
        }
      }

      httpsClient.stop();
    }
  }

  lastCheckDurationMs = millis() - lastCheckStartedAt;
  lastCheckCompletedAt = millis();
  firstCheckCompleted = true;
  checkInProgress = false;

  if (checkSucceeded)
  {
    serviceHealthy = true;
    alarmActive = false;
    buzzerSilenced = false;

    successfulChecks++;
    consecutiveFailures = 0;

    lastError = "";

    Serial.print("Health check succeeded. HTTP ");
    Serial.print(lastHttpStatus);
    Serial.print(", duration ");
    Serial.print(lastCheckDurationMs);
    Serial.println(" ms");
  }
  else
  {
    const bool wasHealthy = serviceHealthy;

    serviceHealthy = false;
    alarmActive = true;

    failedChecks++;
    consecutiveFailures++;

    // A new transition from healthy to failed re-enables the alarm.
    // Repeated failures do not cancel a manual silence.
    if (wasHealthy || failedChecks == 1)
    {
      buzzerSilenced = false;
    }

    Serial.print("Health check failed: ");
    Serial.print(lastError);

    if (lastHttpStatus > 0)
    {
      Serial.print(", HTTP ");
      Serial.print(lastHttpStatus);
    }

    Serial.print(", duration ");
    Serial.print(lastCheckDurationMs);
    Serial.println(" ms");
  }

  return checkSucceeded;
}

// -----------------------------------------------------------------------------
// Buzzer
// -----------------------------------------------------------------------------

void setBuzzer(bool enabled)
{
  digitalWrite(BUZZER_PIN, enabled ? BUZZER_ON : BUZZER_OFF);
}

void updateBuzzer()
{
  const unsigned long now = millis();

  if (buzzerTestActive)
  {
    if (now - buzzerTestStartedAt < BUZZER_TEST_DURATION_MS)
    {
      setBuzzer(true);
      return;
    }

    buzzerTestActive = false;
    setBuzzer(false);
  }

  if (!alarmActive || buzzerSilenced)
  {
    setBuzzer(false);
    return;
  }

  /*
    Alarm pattern:

      0-500 ms:       ON
      500-1000 ms:    OFF
      1000-1500 ms:   ON
      1500-6000 ms:   OFF
  */

  const unsigned long position = now % 6000UL;

  const bool buzzerShouldBeOn =
    position < 500UL ||
    (position >= 1000UL && position < 1500UL);

  setBuzzer(buzzerShouldBeOn);
}

// -----------------------------------------------------------------------------
// API server
// -----------------------------------------------------------------------------

void handleApiClient()
{
  WiFiClient client = apiServer.available();

  if (!client)
  {
    return;
  }

  Serial.println("API client connected.");

  client.setTimeout(1000);

  unsigned long requestStartedAt = millis();

  while (!client.available() &&
         client.connected() &&
         millis() - requestStartedAt < 1000UL)
  {
    updateBuzzer();
  }

  if (!client.available())
  {
    client.stop();
    Serial.println("API request timeout.");
    return;
  }

  String requestLine = client.readStringUntil('\n');
  requestLine.trim();

  Serial.print("API request: ");
  Serial.println(requestLine);

  // Discard the remaining request headers.
  while (client.connected())
  {
    if (!client.available())
    {
      break;
    }

    String headerLine = client.readStringUntil('\n');
    headerLine.trim();

    if (headerLine.length() == 0)
    {
      break;
    }
  }

  const int firstSpace = requestLine.indexOf(' ');
  const int secondSpace = requestLine.indexOf(' ', firstSpace + 1);

  if (firstSpace <= 0 || secondSpace <= firstSpace)
  {
    sendJsonResponse(
      client,
      400,
      "Bad Request",
      "{\"error\":\"Invalid HTTP request\"}"
    );

    client.stop();
    return;
  }

  const String method = requestLine.substring(0, firstSpace);
  const String path = requestLine.substring(firstSpace + 1, secondSpace);

  if (path == "/api/status")
  {
    if (method != "GET")
    {
      sendMethodNotAllowed(client);
    }
    else
    {
      sendStatusResponse(client);
    }
  }
  else if (path == "/api/check")
  {
    if (method != "POST")
    {
      sendMethodNotAllowed(client);
    }
    else
    {
      performHealthCheck();
      sendStatusResponse(client);
    }
  }
  else if (path == "/api/buzzer/test")
  {
    if (method != "POST")
    {
      sendMethodNotAllowed(client);
    }
    else
    {
      buzzerTestActive = true;
      buzzerTestStartedAt = millis();

      sendJsonResponse(
        client,
        200,
        "OK",
        "{\"result\":\"Buzzer test started\"}"
      );
    }
  }
  else if (path == "/api/buzzer/silence")
  {
    if (method != "POST")
    {
      sendMethodNotAllowed(client);
    }
    else
    {
      buzzerSilenced = true;
      setBuzzer(false);

      sendJsonResponse(
        client,
        200,
        "OK",
        "{\"result\":\"Alarm silenced\"}"
      );
    }
  }
  else if (path == "/api/buzzer/unsilence")
  {
    if (method != "POST")
    {
      sendMethodNotAllowed(client);
    }
    else
    {
      buzzerSilenced = false;

      sendJsonResponse(
        client,
        200,
        "OK",
        "{\"result\":\"Alarm enabled\"}"
      );
    }
  }
  else
  {
    sendNotFound(client);
  }

  delay(1);
  client.stop();

  Serial.println("API client disconnected.");
}

// -----------------------------------------------------------------------------
// API responses
// -----------------------------------------------------------------------------

void sendStatusResponse(WiFiClient &client)
{
  IPAddress ip = WiFi.localIP();

  String json;
  json.reserve(600);

  json += "{";

  json += "\"device\":\"UNO R4 WiFi Health Checker\",";

  json += "\"target\":\"https://";
  json += HEALTH_HOST;
  json += HEALTH_PATH;
  json += "\",";

  json += "\"wifi_connected\":";
  json += boolToJson(WiFi.status() == WL_CONNECTED);
  json += ",";

  json += "\"ip_address\":\"";
  json += ip.toString();
  json += "\",";

  json += "\"rssi_dbm\":";
  json += String(WiFi.RSSI());
  json += ",";

  json += "\"first_check_completed\":";
  json += boolToJson(firstCheckCompleted);
  json += ",";

  json += "\"check_in_progress\":";
  json += boolToJson(checkInProgress);
  json += ",";

  json += "\"healthy\":";
  json += boolToJson(serviceHealthy);
  json += ",";

  json += "\"http_status\":";
  json += String(lastHttpStatus);
  json += ",";

  json += "\"last_error\":\"";
  json += escapeJson(lastError);
  json += "\",";

  json += "\"last_check_duration_ms\":";
  json += String(lastCheckDurationMs);
  json += ",";

  json += "\"last_check_completed_at_ms\":";
  json += String(lastCheckCompletedAt);
  json += ",";

  json += "\"successful_checks\":";
  json += String(successfulChecks);
  json += ",";

  json += "\"failed_checks\":";
  json += String(failedChecks);
  json += ",";

  json += "\"consecutive_failures\":";
  json += String(consecutiveFailures);
  json += ",";

  json += "\"alarm_active\":";
  json += boolToJson(alarmActive);
  json += ",";

  json += "\"buzzer_silenced\":";
  json += boolToJson(buzzerSilenced);
  json += ",";

  json += "\"uptime_ms\":";
  json += String(millis());

  json += "}";

  sendJsonResponse(client, 200, "OK", json);
}

void sendJsonResponse(
  WiFiClient &client,
  int statusCode,
  const char *statusText,
  const String &json
)
{
  client.print("HTTP/1.1 ");
  client.print(statusCode);
  client.print(" ");
  client.println(statusText);

  client.println("Content-Type: application/json; charset=utf-8");
  client.println("Cache-Control: no-store");
  client.println("Connection: close");

  client.print("Content-Length: ");
  client.println(json.length());

  client.println();
  client.print(json);
}

void sendNotFound(WiFiClient &client)
{
  sendJsonResponse(
    client,
    404,
    "Not Found",
    "{\"error\":\"API endpoint not found\"}"
  );
}

void sendMethodNotAllowed(WiFiClient &client)
{
  sendJsonResponse(
    client,
    405,
    "Method Not Allowed",
    "{\"error\":\"HTTP method not allowed\"}"
  );
}

// -----------------------------------------------------------------------------
// Utility functions
// -----------------------------------------------------------------------------

String boolToJson(bool value)
{
  return value ? "true" : "false";
}

String escapeJson(const String &value)
{
  String result;
  result.reserve(value.length() + 8);

  for (unsigned int i = 0; i < value.length(); i++)
  {
    const char c = value.charAt(i);

    switch (c)
    {
      case '"':
        result += "\\\"";
        break;

      case '\\':
        result += "\\\\";
        break;

      case '\n':
        result += "\\n";
        break;

      case '\r':
        result += "\\r";
        break;

      case '\t':
        result += "\\t";
        break;

      default:
        result += c;
        break;
    }
  }

  return result;
}