/**
  ******************************************************************************
  * @file           : HttpLineReader.h
  * @brief          : Bounded, watchdog-aware HTTP line reader.
  * @project        : UNO R4 WiFi Multi-Service Health Checker
  * @platform       : Arduino UNO R4 WiFi (Renesas RA4M1)
  * @created        : 27.09.2026
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

#ifndef HTTP_LINE_READER_H
#define HTTP_LINE_READER_H

#include <Arduino.h>
#include <Client.h>
#include "Watchdog.h"

/**
  * @brief Outcome of readHttpLine().
  */
enum class LineReadResult
{
  COMPLETE,   // A complete line was read.
  TRUNCATED,  // The line was longer than the buffer; it was consumed and truncated.
  TIMED_OUT,  // No line terminator before the deadline.
  CLOSED      // The peer closed the connection before a line terminator.
};

/**
  * @brief Read one CRLF- or LF-terminated line with a bounded size and time.
  *
  * Unlike Stream::readStringUntil(), this never allocates heap memory, never
  * accepts more than capacity - 1 characters, and refreshes the watchdog while
  * it waits. Blocks for at most about timeoutMs.
  *
  * @param client (Client&) Connected client to read from.
  * @param buffer (char*) Non-null output buffer. Always NUL-terminated; the
  *        line terminator is not stored.
  * @param capacity (size_t) Size of buffer in bytes; must be at least 1.
  * @param timeoutMs (unsigned long) Maximum time to wait for the whole line.
  * @param watchdog (Watchdog&) Watchdog refreshed while waiting.
  * @retval (LineReadResult) Completion status.
  */
LineReadResult readHttpLine(
  Client &client,
  char *buffer,
  size_t capacity,
  unsigned long timeoutMs,
  Watchdog &watchdog
);

#endif
