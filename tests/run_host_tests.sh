#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/gbareader-tests-XXXXXX")"
trap 'rm -rf "$OUT"' EXIT

CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I"$ROOT/include")

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_reader_core.cpp" "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_reader_core"
"$OUT/test_reader_core"

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_reader_save.cpp" "$ROOT/src/reader_save.cpp" "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_reader_save"
"$OUT/test_reader_save"

python3 "$ROOT/tests/test_source_contracts.py"
