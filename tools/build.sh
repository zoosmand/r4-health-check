#!/usr/bin/env bash
#
# Compile the firmware with arduino-cli.
#
# The sketch lives in src/UNO_R4_Health_Checker/ and is compiled in place.
#
# Usage:
#   tools/build.sh                     compile only
#   tools/build.sh -p PORT             compile and upload to the board on PORT
#   tools/build.sh -- EXTRA_ARGS...    pass extra arguments to
#                                      "arduino-cli compile", for example
#                                      --build-property overrides
#
# Environment:
#   ARDUINO_CLI   arduino-cli executable (default: arduino-cli from PATH)
#   FQBN          board (default: arduino:renesas_uno:unor4wifi)
#
# When src/UNO_R4_Health_Checker/arduino_secrets.h is missing (for example
# in CI), the sketch is copied to build/ and compiled with the example
# credentials; the source tree is not modified.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_NAME="UNO_R4_Health_Checker"
SKETCH="$ROOT/src/$SKETCH_NAME"
OUTPUT="$ROOT/build/output"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN="${FQBN:-arduino:renesas_uno:unor4wifi}"

PORT=""
while getopts "p:" option; do
  case "$option" in
    p) PORT="$OPTARG" ;;
    *) echo "usage: $0 [-p PORT] [-- EXTRA_ARGS...]" >&2; exit 2 ;;
  esac
done
shift $((OPTIND - 1))

mkdir -p "$OUTPUT"

if [[ ! -f "$SKETCH/arduino_secrets.h" ]]; then
  echo "warning: $SKETCH_NAME/arduino_secrets.h not found; building a copy with the example credentials" >&2
  STAGE="$ROOT/build/$SKETCH_NAME"
  rm -rf "$STAGE"
  mkdir -p "$STAGE"
  cp "$SKETCH"/*.ino "$SKETCH"/*.h "$SKETCH"/*.cpp "$STAGE"/
  cp "$SKETCH/arduino_secrets.h.example" "$STAGE/arduino_secrets.h"
  SKETCH="$STAGE"
fi

"$ARDUINO_CLI" compile \
  --fqbn "$FQBN" \
  --warnings all \
  --output-dir "$OUTPUT" \
  "$@" \
  "$SKETCH"

if [[ -n "$PORT" ]]; then
  "$ARDUINO_CLI" upload --fqbn "$FQBN" --port "$PORT" --input-dir "$OUTPUT" "$SKETCH"
fi
