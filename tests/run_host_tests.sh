#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/gbareader-tests-XXXXXX")"
trap 'rm -rf "$OUT"' EXIT

CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I"$ROOT/include")
if [[ -n "${EXTRA_CXXFLAGS:-}" ]]; then
    read -r -a EXTRA_CXXFLAGS_ARRAY <<< "$EXTRA_CXXFLAGS"
    CXXFLAGS+=("${EXTRA_CXXFLAGS_ARRAY[@]}")
fi

g++ "${CXXFLAGS[@]}" "$ROOT/tests/test_reader_crc32.cpp" -o "$OUT/test_reader_crc32"
"$OUT/test_reader_crc32"

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_reader_core.cpp" "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_reader_core"
"$OUT/test_reader_core"

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_reader_ui_state.cpp" "$ROOT/src/reader_ui_state.cpp" \
    -o "$OUT/test_reader_ui_state"
"$OUT/test_reader_ui_state"

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_reader_txt_save.cpp" "$ROOT/src/reader_txt_save.cpp" "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_reader_txt_save"
"$OUT/test_reader_txt_save"

# Compile the production storage branch with only its unused Butano include stubbed.
mkdir -p "$OUT/stubs"
printf '#pragma once\n' > "$OUT/stubs/bn_core.h"
g++ "${CXXFLAGS[@]}" -D__DEVKITARM__ -I"$OUT/stubs" \
    -Wno-misleading-indentation -ffunction-sections -fdata-sections \
    "$ROOT/tests/test_reader_file_ranges.cpp" "$ROOT/src/reader_file.cpp" \
    "$ROOT/src/reader_core.cpp" "$ROOT/src/reader_txt_save.cpp" \
    -Wl,--gc-sections -o "$OUT/test_reader_file_ranges"
"$OUT/test_reader_file_ranges"
g++ "${CXXFLAGS[@]}" -D__DEVKITARM__ -I"$OUT/stubs" \
    -Wno-misleading-indentation -ffunction-sections -fdata-sections \
    "$ROOT/tests/test_library.cpp" "$ROOT/src/reader_file.cpp" \
    "$ROOT/src/reader_core.cpp" "$ROOT/src/reader_txt_save.cpp" \
    -Wl,--gc-sections -o "$OUT/test_library"
"$OUT/test_library"

python3 "$ROOT/tests/generate_epub_fixtures.py" "$OUT/fixtures"
g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_epub.cpp" "$ROOT/src/epub_document.cpp" "$ROOT/src/miniz_tinfl.c" \
    "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_epub"
"$OUT/test_epub" \
    "$OUT/fixtures/stored.epub" "$OUT/fixtures/deflated.epub" "$OUT/fixtures/ordered.epub" \
    "$OUT/fixtures/truncated.epub" "$OUT/fixtures/missing-container.epub" \
    "$OUT/fixtures/missing-rootfile.epub" "$OUT/fixtures/missing-manifest.epub" \
    "$OUT/fixtures/missing-spine.epub" "$OUT/fixtures/encrypted.epub" \
    "$OUT/fixtures/unsupported.epub" "$OUT/fixtures/traversal.epub" \
    "$OUT/fixtures/declared-large.epub" "$OUT/fixtures/extracted-large.epub" \
    "$OUT/fixtures/ignored-asset.epub" "$OUT/fixtures/prefixed-whitespace.epub" \
    "$OUT/fixtures/wrong-media.epub" "$OUT/fixtures/crc-central-mismatch.epub" \
    "$OUT/fixtures/corrupt-payload.epub" "$OUT/fixtures/fake-eocd-comment.epub" \
    "$OUT/fixtures/local-name-mismatch.epub" "$OUT/fixtures/local-flags-mismatch.epub" \
    "$OUT/fixtures/deflate-trailing.epub" "$OUT/fixtures/self-closing-suppressed.epub" \
    "$OUT/fixtures/entities.epub" "$OUT/fixtures/zip64-central-extra.epub" \
    "$OUT/fixtures/zip64-local-extra.epub" "$OUT/fixtures/zip64-locator.epub" \
    "$OUT/fixtures/malformed-central-extra.epub" "$OUT/fixtures/malformed-local-extra.epub" \
    "$OUT/fixtures/scoped-opf.epub" "$OUT/fixtures/quoted-tags.epub" \
    "$OUT/fixtures/unterminated-opf-tag.epub" "$OUT/fixtures/unterminated-xhtml-tag.epub" \
    "$OUT/fixtures/stored.epub" \
    "$OUT/fixtures/local-crc-mismatch.epub" \
    "$OUT/fixtures/local-compressed-size-mismatch.epub" \
    "$OUT/fixtures/local-uncompressed-size-mismatch.epub" \
    "$OUT/fixtures/descriptor-signed.epub" "$OUT/fixtures/descriptor-unsigned.epub" \
    "$OUT/fixtures/descriptor-absent.epub" "$OUT/fixtures/descriptor-bad-crc.epub" \
    "$OUT/fixtures/descriptor-bad-compressed-size.epub" "$OUT/fixtures/descriptor-truncated.epub" \
    "$OUT/fixtures/many-entries.epub" "$OUT/fixtures/ignored-corrupt-image.epub" \
    "$OUT/fixtures/window-cross.epub" "$OUT/fixtures/stream-boundaries.epub" \
    "$OUT/fixtures/window-crc-mismatch.epub" \
    "$OUT/fixtures/percent-encoded-href.epub" \
    "$OUT/fixtures/entity-encoded-href.epub" \
    "$OUT/fixtures/hundred-spine-items.epub" \
    "$OUT/fixtures/multiple-rootfiles.epub" \
    "$OUT/fixtures/required-image-suffix-zip64.epub" \
    "$OUT/fixtures/required-image-suffix-local-crc.epub" \
    "$OUT/fixtures/mixed-case-media.epub" "$OUT/fixtures/percent-traversal.epub" \
    "$OUT/fixtures/long-internal-path.epub" "$OUT/fixtures/visible-cdata.epub" \
    "$OUT/fixtures/semantic-blocks.epub" "$OUT/fixtures/payload-overlap.epub" \
    "$OUT/fixtures/duplicate-required-name.epub" "$OUT/fixtures/large-streamed.epub" \
    "$OUT/fixtures/metadata-too-large.epub" \
    "$OUT/fixtures/compressed-entry-too-large.epub" \
    "$OUT/fixtures/too-many-spine-items.epub" \
    "$OUT/fixtures/appended-cache-compatible.epub" \
    "$OUT/fixtures/glyph-corpus.epub"

g++ "${CXXFLAGS[@]}" "$ROOT/tests/test_reader_file.cpp" "$ROOT/src/reader_file.cpp" \
    "$ROOT/src/reader_txt_save.cpp" "$ROOT/src/reader_core.cpp" \
    "$ROOT/src/epub_document.cpp" "$ROOT/src/miniz_tinfl.c" \
    -o "$OUT/test_reader_file"
"$OUT/test_reader_file" "$OUT/fixtures/ordered.epub" "$OUT/fixtures/cached.epub" \
    "$OUT/fixtures/legacy-v047.epub" "$OUT/fixtures/legacy-v047-migrated.epub" \
    "$OUT/fixtures/unterminated-xhtml-tag.epub" \
    "$OUT/fixtures/stored.epub" "$OUT/fixtures/deflated.epub" \
    "$OUT/fixtures/owned-neighbors.epub" "$OUT/fixtures/neighbors-cached.epub"

python3 "$ROOT/tests/test_standard_zip_cache.py" "$OUT/fixtures" "$OUT/fixtures/cached.epub" "$OUT/fixtures/ordered.epub"
python3 "$ROOT/tests/test_glyph_coverage.py" "$OUT/fixtures"
python3 "$ROOT/references/superfw/res/fonts/generator.py" \
    --font-files "$ROOT/references/superfw/res/fonts/unscii-16-full.hex" \
    --font-blocks reader-punctuation,reader-currency,reader-letterlike \
    --output "$OUT/reader-symbols.pack" >/dev/null
cmp "$ROOT/references/superfw/res/reader-symbols.pack" "$OUT/reader-symbols.pack"

# Exercise the real body bitmap compositor and shipped fonts on a host framebuffer.
gcc -std=c11 "${CXXFLAGS[@]:1}" -Wno-old-style-declaration -Wno-discarded-qualifiers \
    -I"$ROOT/references/superfw/src" -I"$ROOT/references/superfw/src/fonts" \
    -c "$ROOT/src/superfw_font.c" -o "$OUT/font.o"
g++ "${CXXFLAGS[@]}" -I"$ROOT/references/superfw/src/fonts" \
    "$ROOT/tests/test_reader_display.cpp" "$ROOT/src/reader_core.cpp" \
    "$ROOT/src/reader_credits.cpp" "$ROOT/src/reader_controls.cpp" "$OUT/font.o" -o "$OUT/test_reader_display"
"$OUT/test_reader_display" "$ROOT/references/superfw/res/fonts.pack" \
    "$ROOT/references/superfw/res/reader-symbols.pack"

python3 "$ROOT/tests/test_credits_wiring.py"
python3 "$ROOT/tests/test_source_contracts.py"
