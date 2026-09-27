#ifndef STATUS_DISPLAY_H
#define STATUS_DISPLAY_H

#include <Arduino.h>

/*
  Status indication on the UNO R4 WiFi built-in 12 x 8 LED matrix.

  Shows a steady caution sign (a triangle with an exclamation mark) while at
  least one service is failing, and turns the matrix off otherwise.

  The matrix library multiplexes the LEDs from its own timer interrupt, so
  the image stays visible while the main loop is blocked in a health check.
  The image is steady rather than blinking for the same reason: a blink
  driven from loop() would stop during a check.
*/
class StatusDisplay
{
public:
  StatusDisplay();

  /**
    * @brief Start the LED matrix driver and clear the display.
    * @retval (bool) False when no hardware timer was available for the
    *         matrix; the display then stays dark.
    */
  bool begin();

  /**
    * @brief Show or hide the caution sign. Redraws only on a change.
    * @param servicesFailing (bool) True while at least one service is
    *        failing.
    */
  void update(bool servicesFailing);

private:
  bool _running;
  bool _cautionShown;
};

#endif
