# GBA Reader v0.8.0 — block cache, punctuation and credits

## Faster saved-EPUB opening

- New caches verify text in 4 KiB blocks as needed, rather than reading and checksumming the entire book before showing the saved page.
- ZIP/package safety checks, source fingerprint, cache header and the small checksum index remain validated upfront. The ZIP member CRC is reconstructed from the block checksums without scanning text.
- A retained checksum window avoids repeatedly seeking to the end of the cache during page turns. Fixed-block CRC combination uses a 128-byte read-only operator; no whole-book or whole-index allocation is added.
- Bounded central-directory lookup, a combined source-fingerprint pass and bulk metadata reads reduce repeated directory scans and seeks. Hash collisions retain exact duplicate-detecting lookup. The document object grows by a bounded 2,064 bytes; its existing text/inflater workspace is reused.

## GBAWriter-style punctuation

Display-only substitutions use the same plain punctuation glyphs as GBAWriter:

- `‘ ’ ‚` → `'`
- `“ ” „` → `"`
- `‐ ‑` → `-`

Original TXT/EPUB text and normalized cache bytes are not rewritten. Source byte offsets remain valid. Ellipses, en/em dashes, guillemets, bullets, currency and other symbols are unchanged. A checksummed display-layout marker preserves new-layout Back history on reopen while rebuilding older page boundaries once, keeping the saved reading position and settings.

## Credits

Press **Start on the library screen** to show:

```text
Made by Halim Jarrar
(C) 2026
halim-jarrar.de
monday@halim-jarrar.de
```

The library indicates `start: credits` near the bottom. **B or Start** closes credits. Start still saves normally while reading; existing reading/settings controls are retained.

## Upgrading an already-saved EPUB

**Open the book, press Start to save once, then close and reopen it.** That one save upgrades an existing v0.6/v0.7 owned cache to block validation. Until then, the old cache stays readable with its original full-text checks. Subsequent saves in the same session remain state-only, not repeated full-cache writes.

The internal cache-format field is now 6; the existing `META-INF/gbareader/cache-v5` and `state-v5` member names remain. Original EPUB entries remain available to other readers. State and cache stay inside the EPUB, with no permanent sidecars.

## Integrity trade-off

Damage in an unread cached-text block is detected when the block is requested, not at initial open. A failed block is never displayed: the reader switches to validated original chapters, or fails safely if the originals cannot supply compatible text. Saving after successful fallback can replace the damaged cache. As in v0.7, original chapter payload damage with unchanged ZIP metadata is checked only when original chapters are needed. CRCs protect against accidental corruption, not coordinated malicious checksum replacement.

## Measurements

Production ReaderFile/FatFS and EPUB code were compiled on the host and run on FAT16 images. Four synthetic same-content books were saved independently by v0.7 and v0.8, followed by five alternating-order reopens per version. Median open-plus-resume times:

- One chapter: **3.354 → 0.418 ms**.
- 32 chapters: **5.019 → 1.811 ms**.
- 32 chapters with image payloads: **8.414 → 2.926 ms**.
- 128 chapters, approximately 4.77 MB normalized text: **89.036 → 14.271 ms**.

Opening read 48–73% fewer disk bytes in these fixtures. All 40 reopens hit their cache; the next 50 pages had identical text hashes and source offsets, with no writes or image changes. Paging used the same or fewer disk requests, with a small additional block-checksum CPU cost. These are synthetic host measurements with fixed-width test glyphs, **not GBA/Supercard timing promises**; wall-clock results vary with host load, book layout and physical storage.

A separate instrumented 5 MiB fixture requests only its 5,124-byte checksum table during cache open, with no original chapter or cached-text payload requests until text is demanded.

## Validation and limits

- Full host regression and ASan/UBSan suites, including real-font punctuation pixels, wrapping/source offsets, exact credits framebuffer and input-state tests.
- Lazy block reads, checksum-index/header/outer-CRC guards, late corruption/fallback, length mismatch, short reads, duplicate/path/bounds checks and cache migration.
- Production FatFS successful saves, same-object repeated saves and exercised transient-error recovery, with both normal and legacy fixtures, parser readback, independent ZIP CRC checks and read-only filesystem checks.
- Separate sector-backed late-cache-corruption test: identical pages after fallback, no implicit writes, then an explicit save restores a valid standard ZIP while preserving original entries.
- Complete devkitARM/Butano ROM build, GBA header and memory-placement/frame-budget checks, independent code review and exact-release-ROM emulator UI verification.

Physical Supercard hardware testing remains separate. Persistent storage failure, arbitrary power loss and torn sectors are not guaranteed recoverable. Physical EPUB compaction remains deferred: saves append data and remain subject to the existing 128 MiB archive limit. Keep backups of your books.

## Download

Use the attached complete **gbareader.gba** ROM. Copy supported TXT/EPUB books to the root of the Supercard SD card.
