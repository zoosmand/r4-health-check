// Host-side unit tests for src/UNO_R4_Health_Checker/TlsCertificateParser.cpp. Run with test/host/run.sh.

#include "../../src/UNO_R4_Health_Checker/TlsCertificateParser.h"
#include "certificate_fixtures.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
static int checks = 0;

#define EXPECT(condition)                                                    \
  do                                                                         \
  {                                                                          \
    checks++;                                                                \
    if (!(condition))                                                        \
    {                                                                        \
      failures++;                                                            \
      std::printf("%s:%d: expectation failed: %s\n", __FILE__, __LINE__,    \
                  #condition);                                               \
    }                                                                        \
  } while (0)

typedef std::vector<uint8_t> Bytes;

static const uint32_t V1_NOT_AFTER = 2106914219UL;
static const uint32_t V3_NOT_AFTER = 2569154235UL;

static void append24(Bytes &out, size_t value)
{
  out.push_back(static_cast<uint8_t>(value >> 16));
  out.push_back(static_cast<uint8_t>(value >> 8));
  out.push_back(static_cast<uint8_t>(value));
}

static Bytes handshakeMessage(uint8_t type, const Bytes &body)
{
  Bytes out;
  out.push_back(type);
  append24(out, body.size());
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

static Bytes serverHello()
{
  // version, random, empty session id, cipher suite, compression.
  Bytes body = {0x03, 0x03};
  body.insert(body.end(), 32, 0x5A);
  body.push_back(0);
  body.push_back(0xC0);
  body.push_back(0x2F);
  body.push_back(0);
  return handshakeMessage(2, body);
}

static Bytes certificateMessage(const uint8_t *leaf, size_t leafLength)
{
  // The leaf followed by a second (issuer) certificate, as servers send.
  Bytes list;
  append24(list, leafLength);
  list.insert(list.end(), leaf, leaf + leafLength);
  append24(list, sizeof(V3_CERTIFICATE));
  list.insert(list.end(), V3_CERTIFICATE, V3_CERTIFICATE + sizeof(V3_CERTIFICATE));

  Bytes body;
  append24(body, list.size());
  body.insert(body.end(), list.begin(), list.end());
  return handshakeMessage(11, body);
}

/**
  * @brief Wrap a handshake byte stream in records of at most fragmentSize.
  */
static Bytes toRecords(const Bytes &handshake, size_t fragmentSize)
{
  Bytes out;

  for (size_t offset = 0; offset < handshake.size(); offset += fragmentSize)
  {
    const size_t length = std::min(fragmentSize, handshake.size() - offset);
    out.push_back(22);
    out.push_back(0x03);
    out.push_back(0x03);
    out.push_back(static_cast<uint8_t>(length >> 8));
    out.push_back(static_cast<uint8_t>(length));
    out.insert(out.end(), handshake.begin() + offset, handshake.begin() + offset + length);
  }

  return out;
}

static Bytes serverFlight(const uint8_t *leaf, size_t leafLength, size_t fragmentSize)
{
  Bytes handshake = serverHello();
  const Bytes certificate = certificateMessage(leaf, leafLength);
  handshake.insert(handshake.end(), certificate.begin(), certificate.end());

  const Bytes done = handshakeMessage(14, Bytes());
  handshake.insert(handshake.end(), done.begin(), done.end());

  return toRecords(handshake, fragmentSize);
}

static CertificateParseStatus feedInChunks(
  TlsCertificateParser &parser,
  const Bytes &data,
  size_t chunkSize
)
{
  CertificateParseStatus status = parser.status();

  for (size_t offset = 0; offset < data.size(); offset += chunkSize)
  {
    const size_t length = std::min(chunkSize, data.size() - offset);
    status = parser.feed(data.data() + offset, length);
  }

  return status;
}

static void testRealCertificates()
{
  TlsCertificateParser parser;

  const Bytes v1 = serverFlight(V1_CERTIFICATE, sizeof(V1_CERTIFICATE), 16384);
  EXPECT(parser.feed(v1.data(), v1.size()) == CertificateParseStatus::COMPLETE);
  EXPECT(parser.notAfterUnixSeconds() == V1_NOT_AFTER);
  EXPECT(std::strcmp(parser.errorText(), "") == 0);

  parser.reset();
  const Bytes v3 = serverFlight(V3_CERTIFICATE, sizeof(V3_CERTIFICATE), 16384);
  EXPECT(parser.feed(v3.data(), v3.size()) == CertificateParseStatus::COMPLETE);
  EXPECT(parser.notAfterUnixSeconds() == V3_NOT_AFTER);
}

static void testFragmentation()
{
  // Every combination of record fragment size and read chunk size must give
  // the same result, including one byte at a time.
  const size_t fragmentSizes[] = {1, 7, 50, 73, 16384};
  const size_t chunkSizes[] = {1, 3, 128, 100000};

  for (size_t fragmentSize : fragmentSizes)
  {
    const Bytes flight = serverFlight(V3_CERTIFICATE, sizeof(V3_CERTIFICATE), fragmentSize);

    for (size_t chunkSize : chunkSizes)
    {
      TlsCertificateParser parser;
      EXPECT(feedInChunks(parser, flight, chunkSize) == CertificateParseStatus::COMPLETE);
      EXPECT(parser.notAfterUnixSeconds() == V3_NOT_AFTER);
    }
  }
}

static void testIncompleteInput()
{
  const Bytes flight = serverFlight(V1_CERTIFICATE, sizeof(V1_CERTIFICATE), 16384);
  TlsCertificateParser parser;

  // Everything up to, but not including, the last byte of notAfter.
  const char notAfterText[] = "361006135659Z";
  const auto found = std::search(
    flight.begin(), flight.end(), notAfterText, notAfterText + sizeof(notAfterText) - 1
  );
  EXPECT(found != flight.end());

  const size_t notAfterEnd =
    static_cast<size_t>(found - flight.begin()) + sizeof(notAfterText) - 1;
  EXPECT(parser.feed(flight.data(), notAfterEnd - 1) == CertificateParseStatus::IN_PROGRESS);
  EXPECT(parser.feed(flight.data() + notAfterEnd - 1, 1) == CertificateParseStatus::COMPLETE);
  EXPECT(parser.notAfterUnixSeconds() == V1_NOT_AFTER);

  // Bytes after completion are ignored.
  EXPECT(parser.feed(flight.data(), flight.size()) == CertificateParseStatus::COMPLETE);
  EXPECT(parser.notAfterUnixSeconds() == V1_NOT_AFTER);
}

static void testAlert()
{
  // fatal protocol_version, as a TLS 1.3-only server answers.
  const Bytes alert = {21, 0x03, 0x03, 0x00, 0x02, 0x02, 0x46};
  TlsCertificateParser parser;

  EXPECT(parser.feed(alert.data(), alert.size()) == CertificateParseStatus::FAILED);
  EXPECT(std::strcmp(parser.errorText(), "TLS alert from server") == 0);
}

static void testNotTls()
{
  const char response[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
  TlsCertificateParser parser;

  EXPECT(parser.feed(reinterpret_cast<const uint8_t *>(response), sizeof(response) - 1) ==
         CertificateParseStatus::FAILED);
  EXPECT(std::strcmp(parser.errorText(), "Not a TLS handshake response") == 0);
}

static void testNoCertificate()
{
  Bytes handshake = serverHello();
  const Bytes done = handshakeMessage(14, Bytes());
  handshake.insert(handshake.end(), done.begin(), done.end());

  const Bytes flight = toRecords(handshake, 16384);
  TlsCertificateParser parser;

  EXPECT(parser.feed(flight.data(), flight.size()) == CertificateParseStatus::FAILED);
  EXPECT(std::strcmp(parser.errorText(), "Server sent no certificate") == 0);
}

static void testTruncatedCertificate()
{
  // A leaf that ends inside the issuer name.
  Bytes handshake = serverHello();
  const Bytes certificate = certificateMessage(V1_CERTIFICATE, 40);
  handshake.insert(handshake.end(), certificate.begin(), certificate.end());

  const Bytes flight = toRecords(handshake, 16384);
  TlsCertificateParser parser;

  EXPECT(parser.feed(flight.data(), flight.size()) == CertificateParseStatus::FAILED);
  EXPECT(std::strcmp(parser.errorText(), "Certificate ends before notAfter") == 0);
}

static void testMalformedCertificate()
{
  Bytes leaf(V1_CERTIFICATE, V1_CERTIFICATE + sizeof(V1_CERTIFICATE));

  // Turn the outer SEQUENCE into a SET.
  leaf[0] = 0x31;

  const Bytes flight = serverFlight(leaf.data(), leaf.size(), 16384);
  TlsCertificateParser parser;

  EXPECT(parser.feed(flight.data(), flight.size()) == CertificateParseStatus::FAILED);
  EXPECT(std::strcmp(parser.errorText(), "Malformed certificate") == 0);
}

static void testInvalidRecordLength()
{
  const Bytes record = {22, 0x03, 0x03, 0x48, 0x01};
  TlsCertificateParser parser;

  EXPECT(parser.feed(record.data(), record.size()) == CertificateParseStatus::FAILED);
  EXPECT(std::strcmp(parser.errorText(), "Invalid TLS record length") == 0);
}

static void testClientHello()
{
  uint8_t random[TLS_RANDOM_LENGTH];

  for (size_t i = 0; i < sizeof(random); i++)
  {
    random[i] = static_cast<uint8_t>(i);
  }

  uint8_t buffer[512];
  const char host[] = "secure.example.com";
  const size_t length = buildTlsClientHello(host, random, buffer, sizeof(buffer));

  EXPECT(length > 100);

  // Record header and lengths.
  EXPECT(buffer[0] == 22 && buffer[1] == 0x03 && buffer[2] == 0x01);
  EXPECT(static_cast<size_t>((buffer[3] << 8) | buffer[4]) == length - 5);
  EXPECT(buffer[5] == 1);
  EXPECT(static_cast<size_t>((buffer[6] << 16) | (buffer[7] << 8) | buffer[8]) == length - 9);

  // TLS 1.2, the random, and an empty session id.
  EXPECT(buffer[9] == 0x03 && buffer[10] == 0x03);
  EXPECT(std::memcmp(buffer + 11, random, sizeof(random)) == 0);
  EXPECT(buffer[43] == 0);

  // Walk the cipher suites and compression methods to the extensions.
  const size_t suitesLength = static_cast<size_t>((buffer[44] << 8) | buffer[45]);
  EXPECT(suitesLength > 0 && suitesLength % 2 == 0);

  size_t offset = 46 + suitesLength;
  EXPECT(buffer[offset] == 1 && buffer[offset + 1] == 0);
  offset += 2;

  const size_t extensionsLength = static_cast<size_t>((buffer[offset] << 8) | buffer[offset + 1]);
  EXPECT(offset + 2 + extensionsLength == length);
  offset += 2;

  // The first extension is server_name with the host.
  const size_t nameLength = std::strlen(host);
  EXPECT(buffer[offset] == 0 && buffer[offset + 1] == 0);
  EXPECT(static_cast<size_t>((buffer[offset + 2] << 8) | buffer[offset + 3]) == nameLength + 5);
  EXPECT(static_cast<size_t>((buffer[offset + 7] << 8) | buffer[offset + 8]) == nameLength);
  EXPECT(std::memcmp(buffer + offset + 9, host, nameLength) == 0);

  // Every extension length adds up to the extensions block.
  size_t walked = 0;

  while (walked + 4 <= extensionsLength)
  {
    walked += 4 + static_cast<size_t>((buffer[offset + walked + 2] << 8) | buffer[offset + walked + 3]);
  }

  EXPECT(walked == extensionsLength);
}

static void testClientHelloLimits()
{
  uint8_t random[TLS_RANDOM_LENGTH] = {};
  uint8_t buffer[512];

  EXPECT(buildTlsClientHello("", random, buffer, sizeof(buffer)) == 0);
  EXPECT(buildTlsClientHello("bad host", random, buffer, sizeof(buffer)) == 0);
  EXPECT(buildTlsClientHello("https://x", random, buffer, sizeof(buffer)) == 0);

  const size_t length = buildTlsClientHello("example.com", random, buffer, sizeof(buffer));
  EXPECT(length > 0);
  EXPECT(buildTlsClientHello("example.com", random, buffer, length) == length);
  EXPECT(buildTlsClientHello("example.com", random, buffer, length - 1) == 0);

  // The longest valid name still fits the 512-byte buffer.
  char longest[254];
  std::memset(longest, 'a', 253);
  longest[253] = '\0';
  EXPECT(buildTlsClientHello(longest, random, buffer, sizeof(buffer)) > 0);

  char tooLong[255];
  std::memset(tooLong, 'a', 254);
  tooLong[254] = '\0';
  EXPECT(buildTlsClientHello(tooLong, random, buffer, sizeof(buffer)) == 0);
}

int main()
{
  testRealCertificates();
  testFragmentation();
  testIncompleteInput();
  testAlert();
  testNotTls();
  testNoCertificate();
  testTruncatedCertificate();
  testMalformedCertificate();
  testInvalidRecordLength();
  testClientHello();
  testClientHelloLimits();

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
