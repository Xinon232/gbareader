#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/gbareader-sidecar-tests-XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
mkdir -p "$OUT/stubs"
printf '#pragma once\n' > "$OUT/stubs/bn_core.h"
python3 "$ROOT/tests/generate_epub_fixtures.py" "$OUT/fixtures"
FLAGS=(-std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -D__DEVKITARM__ -I"$ROOT/include" -I"$OUT/stubs" -ffunction-sections -fdata-sections)
if [[ -n "${EXTRA_CXXFLAGS:-}" ]]; then
    read -r -a EXTRA <<< "$EXTRA_CXXFLAGS"
    FLAGS+=("${EXTRA[@]}")
fi
g++ "${FLAGS[@]}" "$ROOT/tests/test_reader_sidecar.cpp" "$ROOT/src/reader_file.cpp" \
    "$ROOT/src/reader_core.cpp" "$ROOT/src/reader_txt_save.cpp" "$ROOT/src/epub_document.cpp" \
    "$ROOT/src/miniz_tinfl.c" -Wl,--gc-sections -o "$OUT/test"
g++ -std=c++17 -I"$ROOT/include" "$ROOT/tests/make_legacy_cache.cpp" "$ROOT/src/reader_file.cpp" \
    "$ROOT/src/reader_core.cpp" "$ROOT/src/reader_txt_save.cpp" "$ROOT/src/epub_document.cpp" \
    "$ROOT/src/miniz_tinfl.c" -o "$OUT/legacy"
"$OUT/legacy" "$OUT/fixtures/window-cross.epub" "$OUT/fixtures/embedded.epub"
"$OUT/test" "$OUT/fixtures/window-cross.epub" "$OUT/fixtures/embedded.epub"
