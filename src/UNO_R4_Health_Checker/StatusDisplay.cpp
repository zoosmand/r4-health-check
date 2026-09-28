#include "StatusDisplay.h"

// Arduino_LED_Matrix.h defines static data, so include it in this file only.
#include <Arduino_LED_Matrix.h>
#include <string.h>

namespace
{
constexpr uint8_t MATRIX_ROWS = 8;
constexpr uint8_t MATRIX_COLUMNS = 12;

// Filled caution triangle; the exclamation mark is the unlit gap.
const uint8_t CAUTION_SIGN[MATRIX_ROWS][MATRIX_COLUMNS] = {
  { 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0 },
  { 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0 },
  { 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0 },
  { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 },
  { 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
};

// A corner the caution sign leaves dark, so both can be shown at once.
constexpr uint8_t HEARTBEAT_ROW = 0;
constexpr uint8_t HEARTBEAT_COLUMN = 0;

static_assert(
  HEARTBEAT_ROW < MATRIX_ROWS && HEARTBEAT_COLUMN < MATRIX_COLUMNS,
  "The heartbeat LED must be on the matrix"
);

uint8_t frame[MATRIX_ROWS][MATRIX_COLUMNS];

ArduinoLEDMatrix matrix;
}  // namespace

StatusDisplay::StatusDisplay()
  : _running(false),
    _cautionShown(false),
    _heartbeatLedOn(false)
{
}

bool StatusDisplay::begin()
{
  _running = matrix.begin();

  if (_running)
  {
    matrix.clear();
  }

  return _running;
}

void StatusDisplay::update(bool servicesFailing)
{
  if (!_running || servicesFailing == _cautionShown)
  {
    return;
  }

  _cautionShown = servicesFailing;
  render();
}

void StatusDisplay::setHeartbeat(bool ledOn)
{
  if (!_running || ledOn == _heartbeatLedOn)
  {
    return;
  }

  _heartbeatLedOn = ledOn;
  render();
}

void StatusDisplay::render()
{
  if (_cautionShown)
  {
    memcpy(frame, CAUTION_SIGN, sizeof(frame));
  }
  else
  {
    memset(frame, 0, sizeof(frame));
  }

  frame[HEARTBEAT_ROW][HEARTBEAT_COLUMN] = _heartbeatLedOn ? 1 : 0;

  matrix.renderBitmap(frame, MATRIX_ROWS, MATRIX_COLUMNS);
}
