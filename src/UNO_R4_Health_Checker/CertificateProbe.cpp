#include "CertificateProbe.h"
#include "AppConfig.h"
#include "TlsCertificateParser.h"
#include "WiFiS3.h"

namespace
{
// A ClientHello with a 253-character host name is about 400 bytes.
constexpr size_t CLIENT_HELLO_CAPACITY = 512;
constexpr size_t READ_CHUNK_SIZE = 128;
}  // namespace

bool readCertificateExpiry(
  const char *host,
  uint16_t port,
  unsigned long timeoutMs,
  Watchdog &watchdog,
  uint32_t &notAfterUnixSeconds,
  const char *&errorText
)
{
  // The handshake is abandoned after the Certificate message, so the client
  // random needs no cryptographic quality.
  uint8_t clientRandom[TLS_RANDOM_LENGTH];
  randomSeed(micros());

  for (uint8_t &value : clientRandom)
  {
    value = static_cast<uint8_t>(random(256));
  }

  uint8_t clientHello[CLIENT_HELLO_CAPACITY];
  const size_t helloLength =
    buildTlsClientHello(host, clientRandom, clientHello, sizeof(clientHello));

  if (helloLength == 0)
  {
    errorText = "Invalid host name";
    return false;
  }

  WiFiClient client;

  if (TLS_CONNECT_TIMEOUT_MS > 0)
  {
    client.setConnectionTimeout(TLS_CONNECT_TIMEOUT_MS);
  }

  watchdog.refresh();

  if (!client.connect(host, port))
  {
    errorText = "TCP connection failed";

    // connect() allocates a socket on the Wi-Fi module even when it fails.
    watchdog.refresh();
    client.stop();
    return false;
  }

  watchdog.refresh();

  bool success = false;
  errorText = "";

  if (client.write(clientHello, helloLength) != helloLength)
  {
    errorText = "ClientHello write failed";
  }
  else
  {
    TlsCertificateParser parser;
    uint8_t chunk[READ_CHUNK_SIZE];
    const unsigned long startedAt = millis();

    while (true)
    {
      watchdog.refresh();

      if (millis() - startedAt >= timeoutMs)
      {
        errorText = "Certificate read timeout";
        break;
      }

      const int count = client.read(chunk, sizeof(chunk));

      if (count > 0)
      {
        const CertificateParseStatus status =
          parser.feed(chunk, static_cast<size_t>(count));

        if (status == CertificateParseStatus::COMPLETE)
        {
          notAfterUnixSeconds = parser.notAfterUnixSeconds();
          success = true;
          break;
        }

        if (status == CertificateParseStatus::FAILED)
        {
          errorText = parser.errorText();
          break;
        }

        continue;
      }

      if (!client.connected())
      {
        errorText = "Connection closed before certificate";
        break;
      }

      delay(10);
    }
  }

  // Closing mid-handshake is normal for the server; it logs at most an
  // aborted handshake.
  watchdog.refresh();
  client.stop();
  delay(50);

  return success;
}
