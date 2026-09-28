#!/usr/bin/env bash
#
# Build and run the host-side unit tests with the system C++ compiler.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/host-tests"
CXX="${CXX:-c++}"

mkdir -p "$OUT"

"$CXX" -std=c++17 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined \
  "$ROOT/test/host/text_parsing_test.cpp" \
  "$ROOT/src/UNO_R4_Health_Checker/TextParsing.cpp" \
  -o "$OUT/text_parsing_test"

"$OUT/text_parsing_test"

"$CXX" -std=c++17 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined \
  "$ROOT/test/host/heartbeat_pattern_test.cpp" \
  "$ROOT/src/UNO_R4_Health_Checker/HeartbeatPattern.cpp" \
  -o "$OUT/heartbeat_pattern_test"

"$OUT/heartbeat_pattern_test"
