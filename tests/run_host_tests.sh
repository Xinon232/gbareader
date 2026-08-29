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

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_reader_core.cpp" "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_reader_core"
"$OUT/test_reader_core"

g++ "${CXXFLAGS[@]}" \
    "$ROOT/tests/test_reader_txt_save.cpp" "$ROOT/src/reader_txt_save.cpp" "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_reader_txt_save"
"$OUT/test_reader_txt_save"

g++ "${CXXFLAGS[@]}" "$ROOT/tests/test_reader_file.cpp" "$ROOT/src/reader_file.cpp" \
    "$ROOT/src/reader_txt_save.cpp" "$ROOT/src/reader_core.cpp" \
    -o "$OUT/test_reader_file"
"$OUT/test_reader_file"

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
    "$OUT/fixtures/too-many-spine-items.epub"

python3 "$ROOT/tests/test_source_contracts.py"
