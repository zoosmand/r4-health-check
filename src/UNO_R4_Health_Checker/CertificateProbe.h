/**
  ******************************************************************************
  * @file           : CertificateProbe.h
  * @brief          : Reads a server certificate's expiry over plain TCP.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 09.10.2026
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017-2026 Dmitry Slobodchikov
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#ifndef CERTIFICATE_PROBE_H
#define CERTIFICATE_PROBE_H

#include <Arduino.h>
#include "Watchdog.h"

/*
  Server certificate expiry probe.

  The Wi-Fi module terminates TLS for WiFiSSLClient and does not expose the
  peer certificate. This probe therefore opens a plain TCP connection, sends
  a TLS 1.2 ClientHello, reads the server's plaintext Certificate message
  until the leaf certificate's notAfter field, and closes the connection
  without completing the handshake (see TlsCertificateParser.h).

  A server that only accepts TLS 1.3 answers with an alert, and the probe
  fails with "TLS alert from server".
*/

/**
  * @brief Read the expiry date of a server's leaf certificate. Blocks for up
  *        to TLS_CONNECT_TIMEOUT_MS (connect) plus timeoutMs (response) plus
  *        two Wi-Fi module commands (send, close); the watchdog is refreshed
  *        between the steps and inside the read loop.
  * @param host (const char*) Non-null DNS host name, also sent as SNI.
  * @param port (uint16_t) TCP port, normally 443.
  * @param timeoutMs (unsigned long) Maximum wait for the certificate after
  *        the ClientHello has been sent.
  * @param watchdog (Watchdog&) Watchdog to refresh.
  * @param notAfterUnixSeconds (uint32_t&) Receives the expiry on success.
  * @param errorText (const char*&) Receives a static failure description,
  *        or an empty string on success.
  * @retval (bool) True when the expiry date was read.
  */
bool readCertificateExpiry(
  const char *host,
  uint16_t port,
  unsigned long timeoutMs,
  Watchdog &watchdog,
  uint32_t &notAfterUnixSeconds,
  const char *&errorText
);

#endif
