#include "StatusDisplay.h"

// Arduino_LED_Matrix.h defines static data, so include it in this file only.
#include <Arduino_LED_Matrix.h>

namespace
{
constexpr uint8_t MATRIX_ROWS = 8;
constexpr uint8_t MATRIX_COLUMNS = 12;

// Filled caution triangle; the exclamation mark is the unlit gap.
uint8_t CAUTION_SIGN[MATRIX_ROWS][MATRIX_COLUMNS] = {
  { 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0 },
  { 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0 },
  { 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0 },
  { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 },
  { 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
};

ArduinoLEDMatrix matrix;
}  // namespace

StatusDisplay::StatusDisplay()
  : _running(false),
    _cautionShown(false)
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

  if (servicesFailing)
  {
    matrix.renderBitmap(CAUTION_SIGN, MATRIX_ROWS, MATRIX_COLUMNS);
  }
  else
  {
    matrix.clear();
  }
}
