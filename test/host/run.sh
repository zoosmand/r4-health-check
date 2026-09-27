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
  "$ROOT/src/TextParsing.cpp" \
  -o "$OUT/text_parsing_test"

"$OUT/text_parsing_test"
