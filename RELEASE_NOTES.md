# GBA Reader v0.6.0 — EPUB efficiency and save recovery

## Changes

- ZIP filename validation reuses the central-directory filename already in memory instead of alternating distant file reads per character.
- EPUB cache preparation exports normalized text sequentially through bounded buffers. Checksums are calculated during export; headers are finalized before publishing the new ZIP directory.
- Valid caches skip repeated XHTML normalization while retaining ZIP/package validation, required original-content CRC checks, and complete DEFLATE-stream checks. Invalid caches fall back to the original EPUB.
- New bookmark directories retain only the newest state entry. Regenerating an invalid cache replaces its live cache entry while retaining original and unknown entries.
- EPUB rollback reacquires the FatFS handle after transient I/O errors, avoiding the latched-error condition that previously prevented truncation. Legacy migration appends after the physical EOF so old trailer/bookmark bytes remain intact on failure.
- Unchanged settings preserve Back history and reconstruction state. Intermediate settings edits no longer paginate an invisible preview.
- Pagination checks vertical space before processing a line that cannot fit.
- CI covers main, pull requests, the release branch and version tags, including host/sanitizer tests, real-FatFS recovery tests and the ROM build. Prior releases are not rewritten.

## Compatibility and limits

- Existing v0.5 ZIP cache/state members and TXT v1/v2/v3 bookmarks remain supported. State stays embedded in the book; no permanent sidecars are introduced.
- Physical compaction is deferred. EPUB files still grow with appended saves, but obsolete state entries no longer accumulate in the live directory. Saves are rejected before exceeding the reader's archive-size limit.
- Cached opening still reads and validates original required content; this is a conservative integrity-preserving path, not unchecked cache trust.
- Persistent storage failure, arbitrary power loss, torn sectors and physical Supercard behavior are not guaranteed by host/emulator checks. The existing TXT footer path can lose bookmark metadata under persistent failure or abrupt interruption.
- No speed benchmarks were performed and no percentage speedup is claimed.

## Validation

Host regression tests and ASAN/UBSAN, standard ZIP validation, successful-save and exercised transient-error recovery checks against real FatFS images (including legacy EPUB migration), ROM build/header checks, and isolated mGBA library-screen boot. Physical Supercard SD testing remains separate. The inherited `make test` target alone is not used as release evidence.

## Download

Use the attached complete `gbareader.gba` ROM. Copy supported TXT/EPUB books to the root of the Supercard SD card.
