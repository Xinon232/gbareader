# GBA Reader v0.7.0 — faster saved-EPUB opening

## Changes

- A valid EPUB text cache is now the fast opening path: validate ZIP/package metadata, required-entry structure and size limits, cache/text versions, source fingerprint and cached-text integrity, then restore the saved page without decompressing the original spine chapters again.
- Cached text is checksummed once. Its finalized CRC is mathematically combined with the header CRC to verify the entire stored ZIP member, preserving both checks without hashing the text twice.
- Shared table-based CRC uses only a 64-byte read-only ROM table, with no whole-book allocation or mutable initialization.
- EPUB reads now use block operations. ReaderFile copies contiguous blocks from its existing 8 KiB window rather than dispatching each byte separately; bounded reads and failure handling remain tested.
- Missing, stale or corrupt caches still fall back to original chapter parsing, CRC and complete DEFLATE-stream validation, followed by the existing cache-preparation path.

## What stays the same

Fast cached page turning, reading position/history, bookmark-only saves, settings, controls, source-document persistence and standard EPUB ZIP members are retained. Existing v0.5/v0.6 v5 caches and saved state are compatible; no resave or conversion is required for an already-valid v0.6 cache. Original EPUB content remains in the file for other e-readers. No permanent sidecars are introduced.

## Integrity policy

A validated cache is treated as the verified reading copy. Corruption confined to original chapter payload bytes, with unchanged ZIP metadata, is deliberately not detected on a valid-cache open. It is checked when those original chapters must be processed again. Cache integrity, package metadata and ZIP safety checks are not skipped. This changes when damage is detected; it does not damage or remove the original EPUB.

## Measurements

Actual production ReaderFile/FatFS and EPUB code were compiled on the host and run against the same FAT16 images containing books already saved by v0.6. Five alternating-order reopens per version and fixture produced these median total open/resume times:

- One-chapter synthetic book: v0.6 **13.018 ms**, v0.7 **1.810 ms**.
- 32-chapter synthetic book: v0.6 **32.066 ms**, v0.7 **8.113 ms**.
- 32-chapter book with image payloads: v0.6 **20.097 ms**, v0.7 **4.138 ms**.

All 30 opens hit the existing cache. Fifty subsequent pages had identical text hashes and offsets between versions; the benchmarks performed no writes, and the FAT images remained byte-identical. These are synthetic, host-side CPU/filesystem measurements with fixed-width test glyphs, not GBA/Supercard timing promises. Physical SD latency and the particular EPUB will affect the improvement.

## Validation

- Full host regression suite and AddressSanitizer/UndefinedBehaviorSanitizer suite.
- Independent CRC reference vectors, block-read bounds/short-read/error tests, and cache-hit tests that deny original chapter payload reads.
- Cache invalidation and original-fallback corruption tests; required ZIP/package safety tests including a cache with a matching source fingerprint.
- Standard ZIP interoperability checks; production FatFS successful saves and exercised transient-error recovery, including legacy EPUB migration.
- Complete devkitARM/Butano ROM build and GBA header validation; independent code review.

Physical Supercard hardware testing remains separate. Persistent storage failure, arbitrary power loss and torn sectors are not guaranteed recoverable. Physical EPUB compaction remains deferred; repeated saves still append data subject to the existing archive-size limit.

## Download

Use the attached complete `gbareader.gba` ROM. Copy supported TXT/EPUB books to the root of the Supercard SD card. Keep backups of books as usual.
