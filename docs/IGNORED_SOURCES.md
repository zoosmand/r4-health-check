# Third-party sources

This repository contains no vendored third-party code. The firmware depends
only on the Arduino board package for the UNO R4, which is installed outside
the repository and must not be modified.

| Dependency | Provides | Installation |
|---|---|---|
| `arduino:renesas_uno` board package | Arduino core, FSP/CMSIS headers, `FspTimer`, and the `WDT`, `WiFiS3`, and `Arduino_LED_Matrix` libraries | `arduino-cli core install arduino:renesas_uno` or Arduino IDE *Boards Manager → Arduino UNO R4 Boards* |
| UNO R4 WiFi module firmware | TLS, DNS, and Wi-Fi handled by the ESP32-S3 coprocessor | Arduino IDE *Tools → Firmware Updater* |
| `arduino-cli` | Command-line build used by `tools/build.sh` | <https://arduino.github.io/arduino-cli/> |

The board package was version 1.6.0 at the time of writing. Record the
package version and the module firmware version in the pull request when
either changes.

## Locally provisioned files

These files are ignored by git and must be created on each machine:

- `src/UNO_R4_Health_Checker/arduino_secrets.h`: Wi-Fi credentials and the optional
  API token. Create it from `arduino_secrets.h.example` in the same folder.

## Generated files

`tools/build.sh` and `test/host/run.sh` write only to `build/`, which is
ignored by git.

---

&copy; 2026, Askug Ltd., Dmitry Slobodchikov
