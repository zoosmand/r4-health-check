#!/usr/bin/env bash
#
# Compile the firmware with arduino-cli.
#
# Arduino tools require the sketch folder to have the same name as the .ino
# file, while this repository keeps the sources in src/. The script copies
# src/ into build/UNO_R4_Health_Checker/ and compiles that copy.
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
# When src/arduino_secrets.h is missing (for example in CI), the example
# file is used for the build copy only.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_NAME="UNO_R4_Health_Checker"
STAGE="$ROOT/build/$SKETCH_NAME"
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

rm -rf "$STAGE"
mkdir -p "$STAGE" "$OUTPUT"
cp "$ROOT"/src/*.ino "$ROOT"/src/*.h "$ROOT"/src/*.cpp "$STAGE"/

if [[ ! -f "$STAGE/arduino_secrets.h" ]]; then
  echo "warning: src/arduino_secrets.h not found; building with the example credentials" >&2
  cp "$ROOT/src/arduino_secrets.h.example" "$STAGE/arduino_secrets.h"
fi

"$ARDUINO_CLI" compile \
  --fqbn "$FQBN" \
  --warnings all \
  --output-dir "$OUTPUT" \
  "$@" \
  "$STAGE"

if [[ -n "$PORT" ]]; then
  "$ARDUINO_CLI" upload --fqbn "$FQBN" --port "$PORT" --input-dir "$OUTPUT" "$STAGE"
fi
