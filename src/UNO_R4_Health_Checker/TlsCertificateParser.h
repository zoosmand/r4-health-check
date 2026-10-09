#ifndef TLS_CERTIFICATE_PARSER_H
#define TLS_CERTIFICATE_PARSER_H

/*
  Read the expiry date of a server certificate without completing a TLS
  handshake.

  In TLS 1.2 the server sends its Certificate message in plaintext right
  after ServerHello. The caller sends the ClientHello built by
  buildTlsClientHello() over a plain TCP connection, feeds every received
  byte to TlsCertificateParser, and closes the connection once the parser
  has finished. No cryptography is involved, and the certificate is not
  validated: this only reports the date. The regular HTTPS check, which runs
  on the Wi-Fi module, still validates the chain.

  The parser streams: it never buffers a record or the certificate and uses
  about 40 bytes of state. Only the first (leaf) certificate is read, and
  only up to its notAfter field.

  Nothing in this module depends on Arduino headers.
*/

#include <stddef.h>
#include <stdint.h>

constexpr size_t TLS_RANDOM_LENGTH = 32;

/**
  * @brief Build a TLS 1.2 ClientHello record with SNI.
  * @param serverName (const char*) Non-null DNS host name, 1..253 characters.
  * @param random (const uint8_t*) Non-null TLS_RANDOM_LENGTH bytes. They need
  *        not be cryptographically random, since the handshake is abandoned.
  * @param buffer (uint8_t*) Non-null output buffer.
  * @param capacity (size_t) Size of buffer; 512 bytes fit any host name.
  * @retval (size_t) Record length in bytes, or 0 when the host name is
  *         invalid or the record does not fit.
  */
size_t buildTlsClientHello(
  const char *serverName,
  const uint8_t *random,
  uint8_t *buffer,
  size_t capacity
);

enum class CertificateParseStatus
{
  IN_PROGRESS,  // More bytes are needed.
  COMPLETE,     // notAfterUnixSeconds() is valid.
  FAILED        // errorText() describes why.
};

class TlsCertificateParser
{
public:
  TlsCertificateParser();

  /**
    * @brief Start over for a new connection.
    */
  void reset();

  /**
    * @brief Consume received bytes. Bytes after COMPLETE or FAILED are
    *        ignored.
    * @param data (const uint8_t*) Bytes from the server; may be nullptr
    *        when length is 0.
    * @param length (size_t) Number of bytes.
    * @retval (CertificateParseStatus) Status after these bytes.
    */
  CertificateParseStatus feed(const uint8_t *data, size_t length);

  CertificateParseStatus status() const;

  /**
    * @brief Expiry of the leaf certificate.
    * @retval (uint32_t) Unix seconds; valid only when status() is COMPLETE.
    */
  uint32_t notAfterUnixSeconds() const;

  /**
    * @brief Static description of the failure, or an empty string.
    */
  const char *errorText() const;

private:
  enum class RecordState
  {
    HEADER,
    BODY
  };

  enum class HandshakeState
  {
    HEADER,
    SKIP_BODY,
    CERTIFICATE_LIST_LENGTH,
    CERTIFICATE_LENGTH,
    CERTIFICATE_BODY
  };

  enum class DerState
  {
    TAG,
    LENGTH,
    LENGTH_BYTES,
    SKIP,
    CAPTURE
  };

  void feedByte(uint8_t value);
  void feedHandshakeByte(uint8_t value);
  void feedCertificateByte(uint8_t value);

  /**
    * @brief Act on a complete DER tag and length for the current step.
    */
  void handleDerElement();

  void fail(const char *errorText);

  CertificateParseStatus _status;
  const char *_errorText;
  uint32_t _notAfterUnixSeconds;

  RecordState _recordState;
  uint8_t _recordHeader[5];
  uint8_t _recordHeaderLength;
  uint16_t _recordRemaining;

  HandshakeState _handshakeState;
  uint8_t _handshakeType;
  uint8_t _fieldLength;
  uint32_t _fieldValue;
  uint32_t _handshakeRemaining;
  uint32_t _certificateRemaining;

  DerState _derState;
  uint8_t _derStep;
  uint8_t _derTag;
  uint8_t _derLengthBytes;
  uint32_t _derLength;
  uint32_t _derRemaining;

  char _timeText[15];
  uint8_t _timeLength;
};

#endif
