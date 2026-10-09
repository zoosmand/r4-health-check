#include "TlsCertificateParser.h"
#include "UtcTime.h"

#include <string.h>

namespace
{
constexpr uint8_t RECORD_TYPE_ALERT = 21;
constexpr uint8_t RECORD_TYPE_HANDSHAKE = 22;

constexpr uint8_t HANDSHAKE_TYPE_HELLO_REQUEST = 0;
constexpr uint8_t HANDSHAKE_TYPE_SERVER_HELLO = 2;
constexpr uint8_t HANDSHAKE_TYPE_CERTIFICATE = 11;

// TLSCiphertext may exceed 2^14 by up to 2048 bytes (RFC 5246, 6.2.3).
constexpr uint16_t MAX_RECORD_LENGTH = 16384U + 2048U;

constexpr uint8_t DER_SEQUENCE = 0x30;
constexpr uint8_t DER_INTEGER = 0x02;
constexpr uint8_t DER_CONTEXT_0 = 0xA0;
constexpr uint8_t DER_UTC_TIME = 0x17;
constexpr uint8_t DER_GENERALIZED_TIME = 0x18;

// Path through the certificate to notAfter (RFC 5280, 4.1):
//   Certificate SEQUENCE { TBSCertificate SEQUENCE { [0] version OPTIONAL,
//   serialNumber, signature, issuer, validity SEQUENCE { notBefore,
//   notAfter } ... } ... }
enum DerStep : uint8_t
{
  STEP_CERTIFICATE,
  STEP_TBS_CERTIFICATE,
  STEP_VERSION_OR_SERIAL,
  STEP_SERIAL,
  STEP_SIGNATURE,
  STEP_ISSUER,
  STEP_VALIDITY,
  STEP_NOT_BEFORE,
  STEP_NOT_AFTER
};

// Offered TLS 1.2 cipher suites. ECDSA and RSA variants are both offered so
// that a server with two certificates picks one as a browser would.
const uint16_t CIPHER_SUITES[] = {
  0xC02B,  // ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
  0xC02F,  // ECDHE_RSA_WITH_AES_128_GCM_SHA256
  0xC02C,  // ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
  0xC030,  // ECDHE_RSA_WITH_AES_256_GCM_SHA384
  0xCCA9,  // ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
  0xCCA8,  // ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
  0xC009,  // ECDHE_ECDSA_WITH_AES_128_CBC_SHA
  0xC013,  // ECDHE_RSA_WITH_AES_128_CBC_SHA
  0xC00A,  // ECDHE_ECDSA_WITH_AES_256_CBC_SHA
  0xC014,  // ECDHE_RSA_WITH_AES_256_CBC_SHA
  0x009C,  // RSA_WITH_AES_128_GCM_SHA256
  0x009D,  // RSA_WITH_AES_256_GCM_SHA384
  0x002F,  // RSA_WITH_AES_128_CBC_SHA
  0x0035   // RSA_WITH_AES_256_CBC_SHA
};

const uint16_t SUPPORTED_GROUPS[] = {
  0x001D,  // x25519
  0x0017,  // secp256r1
  0x0018   // secp384r1
};

const uint16_t SIGNATURE_ALGORITHMS[] = {
  0x0403,  // ecdsa_secp256r1_sha256
  0x0804,  // rsa_pss_rsae_sha256
  0x0401,  // rsa_pkcs1_sha256
  0x0503,  // ecdsa_secp384r1_sha384
  0x0805,  // rsa_pss_rsae_sha384
  0x0501,  // rsa_pkcs1_sha384
  0x0806,  // rsa_pss_rsae_sha512
  0x0601,  // rsa_pkcs1_sha512
  0x0201   // rsa_pkcs1_sha1
};

/**
  * @brief Bounded big-endian writer. After an overflow every write is
  *        ignored and isValid() stays false.
  */
class RecordWriter
{
public:
  RecordWriter(uint8_t *buffer, size_t capacity)
    : _buffer(buffer), _capacity(capacity), _length(0), _valid(true)
  {
  }

  void put8(uint32_t value)
  {
    if (_length >= _capacity)
    {
      _valid = false;
      return;
    }

    _buffer[_length++] = static_cast<uint8_t>(value);
  }

  void put16(uint32_t value)
  {
    put8(value >> 8);
    put8(value);
  }

  void put24(uint32_t value)
  {
    put8(value >> 16);
    put16(value);
  }

  void putBytes(const void *data, size_t length)
  {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);

    for (size_t i = 0; i < length; i++)
    {
      put8(bytes[i]);
    }
  }

  void putList16(const uint16_t *values, size_t count)
  {
    put16(count * 2U);

    for (size_t i = 0; i < count; i++)
    {
      put16(values[i]);
    }
  }

  /**
    * @brief Reserve a length field to be filled in by closeLength().
    * @retval (size_t) Position of the field.
    */
  size_t openLength(size_t width)
  {
    const size_t position = _length;

    for (size_t i = 0; i < width; i++)
    {
      put8(0);
    }

    return position;
  }

  /**
    * @brief Store the number of bytes written after the field.
    */
  void closeLength(size_t position, size_t width)
  {
    if (!_valid)
    {
      return;
    }

    size_t value = _length - position - width;

    for (size_t i = width; i > 0; i--)
    {
      _buffer[position + i - 1] = static_cast<uint8_t>(value);
      value >>= 8;
    }
  }

  bool isValid() const
  {
    return _valid;
  }

  size_t length() const
  {
    return _length;
  }

private:
  uint8_t *_buffer;
  size_t _capacity;
  size_t _length;
  bool _valid;
};

bool isValidHostName(const char *name, size_t length)
{
  if (length == 0 || length > 253)
  {
    return false;
  }

  for (size_t i = 0; i < length; i++)
  {
    const char c = name[i];
    const bool allowed =
      (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') || c == '-' || c == '.';

    if (!allowed)
    {
      return false;
    }
  }

  return true;
}
}  // namespace

size_t buildTlsClientHello(
  const char *serverName,
  const uint8_t *random,
  uint8_t *buffer,
  size_t capacity
)
{
  const size_t nameLength = strlen(serverName);

  if (!isValidHostName(serverName, nameLength))
  {
    return 0;
  }

  RecordWriter writer(buffer, capacity);

  // Record header. Version 3.1 in the record layer is what most clients send
  // for the first record.
  writer.put8(RECORD_TYPE_HANDSHAKE);
  writer.put16(0x0301);
  const size_t recordLength = writer.openLength(2);

  // Handshake header: ClientHello.
  writer.put8(1);
  const size_t handshakeLength = writer.openLength(3);

  writer.put16(0x0303);  // client_version: TLS 1.2
  writer.putBytes(random, TLS_RANDOM_LENGTH);
  writer.put8(0);        // empty session_id

  writer.putList16(CIPHER_SUITES, sizeof(CIPHER_SUITES) / sizeof(CIPHER_SUITES[0]));

  writer.put8(1);        // compression_methods: null only
  writer.put8(0);

  const size_t extensionsLength = writer.openLength(2);

  // server_name
  writer.put16(0x0000);
  writer.put16(nameLength + 5U);
  writer.put16(nameLength + 3U);
  writer.put8(0);        // host_name
  writer.put16(nameLength);
  writer.putBytes(serverName, nameLength);

  // supported_groups
  writer.put16(0x000A);
  writer.put16(sizeof(SUPPORTED_GROUPS) + 2U);
  writer.putList16(SUPPORTED_GROUPS, sizeof(SUPPORTED_GROUPS) / sizeof(SUPPORTED_GROUPS[0]));

  // ec_point_formats: uncompressed
  writer.put16(0x000B);
  writer.put16(2);
  writer.put8(1);
  writer.put8(0);

  // signature_algorithms
  writer.put16(0x000D);
  writer.put16(sizeof(SIGNATURE_ALGORITHMS) + 2U);
  writer.putList16(
    SIGNATURE_ALGORITHMS,
    sizeof(SIGNATURE_ALGORITHMS) / sizeof(SIGNATURE_ALGORITHMS[0])
  );

  // extended_master_secret
  writer.put16(0x0017);
  writer.put16(0);

  // renegotiation_info: empty, as for an initial handshake
  writer.put16(0xFF01);
  writer.put16(1);
  writer.put8(0);

  writer.closeLength(extensionsLength, 2);
  writer.closeLength(handshakeLength, 3);
  writer.closeLength(recordLength, 2);

  return writer.isValid() ? writer.length() : 0;
}

TlsCertificateParser::TlsCertificateParser()
{
  reset();
}

void TlsCertificateParser::reset()
{
  _status = CertificateParseStatus::IN_PROGRESS;
  _errorText = "";
  _notAfterUnixSeconds = 0;

  _recordState = RecordState::HEADER;
  _recordHeaderLength = 0;
  _recordRemaining = 0;

  _handshakeState = HandshakeState::HEADER;
  _handshakeType = 0;
  _fieldLength = 0;
  _fieldValue = 0;
  _handshakeRemaining = 0;
  _certificateRemaining = 0;

  _derState = DerState::TAG;
  _derStep = STEP_CERTIFICATE;
  _derTag = 0;
  _derLengthBytes = 0;
  _derLength = 0;
  _derRemaining = 0;

  _timeLength = 0;
}

CertificateParseStatus TlsCertificateParser::feed(const uint8_t *data, size_t length)
{
  for (size_t i = 0; i < length && _status == CertificateParseStatus::IN_PROGRESS; i++)
  {
    feedByte(data[i]);
  }

  return _status;
}

CertificateParseStatus TlsCertificateParser::status() const
{
  return _status;
}

uint32_t TlsCertificateParser::notAfterUnixSeconds() const
{
  return _notAfterUnixSeconds;
}

const char *TlsCertificateParser::errorText() const
{
  return _errorText;
}

void TlsCertificateParser::fail(const char *errorText)
{
  _status = CertificateParseStatus::FAILED;
  _errorText = errorText;
}

void TlsCertificateParser::feedByte(uint8_t value)
{
  if (_recordState == RecordState::HEADER)
  {
    _recordHeader[_recordHeaderLength++] = value;

    if (_recordHeaderLength < sizeof(_recordHeader))
    {
      return;
    }

    const uint8_t type = _recordHeader[0];
    const uint16_t length =
      static_cast<uint16_t>((_recordHeader[3] << 8) | _recordHeader[4]);

    if (type == RECORD_TYPE_ALERT)
    {
      // Typically protocol_version from a TLS 1.3-only server.
      fail("TLS alert from server");
      return;
    }

    if (type != RECORD_TYPE_HANDSHAKE || _recordHeader[1] != 3)
    {
      fail("Not a TLS handshake response");
      return;
    }

    if (length == 0 || length > MAX_RECORD_LENGTH)
    {
      fail("Invalid TLS record length");
      return;
    }

    _recordRemaining = length;
    _recordState = RecordState::BODY;
    return;
  }

  // Handshake messages may span records; the record layer only strips the
  // headers.
  feedHandshakeByte(value);

  if (--_recordRemaining == 0)
  {
    _recordState = RecordState::HEADER;
    _recordHeaderLength = 0;
  }
}

void TlsCertificateParser::feedHandshakeByte(uint8_t value)
{
  switch (_handshakeState)
  {
    case HandshakeState::HEADER:
      if (_fieldLength == 0)
      {
        _handshakeType = value;
        _fieldValue = 0;
      }
      else
      {
        _fieldValue = (_fieldValue << 8) | value;
      }

      if (++_fieldLength < 4)
      {
        return;
      }

      _fieldLength = 0;
      _handshakeRemaining = _fieldValue;

      if (_handshakeType == HANDSHAKE_TYPE_SERVER_HELLO && _handshakeRemaining > 0)
      {
        _handshakeState = HandshakeState::SKIP_BODY;
      }
      else if (_handshakeType == HANDSHAKE_TYPE_CERTIFICATE)
      {
        _handshakeState = HandshakeState::CERTIFICATE_LIST_LENGTH;
      }
      else if (_handshakeType == HANDSHAKE_TYPE_HELLO_REQUEST && _handshakeRemaining == 0)
      {
        // Ignored, as RFC 5246 allows during a handshake.
      }
      else
      {
        fail("Server sent no certificate");
      }
      return;

    case HandshakeState::SKIP_BODY:
      if (--_handshakeRemaining == 0)
      {
        _handshakeState = HandshakeState::HEADER;
      }
      return;

    case HandshakeState::CERTIFICATE_LIST_LENGTH:
    case HandshakeState::CERTIFICATE_LENGTH:
      _fieldValue = _fieldLength == 0 ? value : (_fieldValue << 8) | value;

      if (++_fieldLength < 3)
      {
        return;
      }

      _fieldLength = 0;

      if (_handshakeState == HandshakeState::CERTIFICATE_LIST_LENGTH)
      {
        if (_fieldValue < 4)
        {
          fail("Empty certificate list");
          return;
        }

        _handshakeState = HandshakeState::CERTIFICATE_LENGTH;
        return;
      }

      if (_fieldValue == 0)
      {
        fail("Empty certificate");
        return;
      }

      _certificateRemaining = _fieldValue;
      _handshakeState = HandshakeState::CERTIFICATE_BODY;
      return;

    case HandshakeState::CERTIFICATE_BODY:
      feedCertificateByte(value);

      if (_status == CertificateParseStatus::IN_PROGRESS && --_certificateRemaining == 0)
      {
        fail("Certificate ends before notAfter");
      }
      return;
  }
}

void TlsCertificateParser::feedCertificateByte(uint8_t value)
{
  switch (_derState)
  {
    case DerState::TAG:
      _derTag = value;
      _derState = DerState::LENGTH;
      return;

    case DerState::LENGTH:
      if (value < 0x80)
      {
        _derLength = value;
        handleDerElement();
        return;
      }

      // Long form. DER forbids the indefinite form (0x80), and no
      // certificate element needs more than three length bytes.
      _derLengthBytes = value & 0x7F;

      if (_derLengthBytes == 0 || _derLengthBytes > 3)
      {
        fail("Malformed certificate");
        return;
      }

      _derLength = 0;
      _derState = DerState::LENGTH_BYTES;
      return;

    case DerState::LENGTH_BYTES:
      _derLength = (_derLength << 8) | value;

      if (--_derLengthBytes == 0)
      {
        handleDerElement();
      }
      return;

    case DerState::SKIP:
      if (--_derRemaining == 0)
      {
        _derState = DerState::TAG;
      }
      return;

    case DerState::CAPTURE:
      _timeText[_timeLength++] = static_cast<char>(value);

      if (_timeLength < _derLength)
      {
        return;
      }

      if (!parseAsn1Time(_timeText, _timeLength, _notAfterUnixSeconds))
      {
        fail("Invalid certificate expiry date");
        return;
      }

      _status = CertificateParseStatus::COMPLETE;
      return;
  }
}

void TlsCertificateParser::handleDerElement()
{
  uint8_t expectedTag = DER_SEQUENCE;
  bool enter = false;
  uint8_t nextStep = static_cast<uint8_t>(_derStep + 1);

  switch (_derStep)
  {
    case STEP_CERTIFICATE:
    case STEP_TBS_CERTIFICATE:
    case STEP_VALIDITY:
      enter = true;
      break;

    case STEP_VERSION_OR_SERIAL:
      if (_derTag == DER_CONTEXT_0)
      {
        expectedTag = DER_CONTEXT_0;
      }
      else
      {
        // No version field (v1 certificate): this is the serial number.
        expectedTag = DER_INTEGER;
        nextStep = STEP_SIGNATURE;
      }
      break;

    case STEP_SERIAL:
      expectedTag = DER_INTEGER;
      break;

    case STEP_NOT_BEFORE:
    case STEP_NOT_AFTER:
      expectedTag = _derTag == DER_GENERALIZED_TIME ? DER_GENERALIZED_TIME : DER_UTC_TIME;
      break;

    default:
      break;
  }

  if (_derTag != expectedTag)
  {
    fail("Malformed certificate");
    return;
  }

  _derStep = nextStep;

  if (enter)
  {
    _derState = DerState::TAG;
    return;
  }

  if (_derStep > STEP_NOT_AFTER)
  {
    // This element is notAfter itself.
    if (_derLength != 13 && _derLength != 15)
    {
      fail("Invalid certificate expiry date");
      return;
    }

    _timeLength = 0;
    _derState = DerState::CAPTURE;
    return;
  }

  _derRemaining = _derLength;
  _derState = _derRemaining == 0 ? DerState::TAG : DerState::SKIP;
}
