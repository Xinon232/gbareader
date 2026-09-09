# gbareader v1.1

Read TXT and EPUB books on your Game Boy Advance. Save your place and return to it later. Put your books in `/gbareader` on your Supercard SD card; they appear directly on the home screen.

The app uses the SD/FatFS and font foundation of [`gba-vocab-trainer-CC` v0.2.5](https://github.com/Xinon232/gba-vocab-trainer-CC/releases/tag/v0.2.5). The home-screen title is `gbareader V1.1`; this work retains the v0.8.0 reading and embedded-save engine.

Version 1.1 opens UTF-8 `.txt` and a deliberately bounded, text-only subset of UTF-8 EPUB 2/3 files directly from a Supercard SD card.

## v1.1 changes

Home shows five books at once instead of four. The list moves slightly up and the navigation instructions move down; title/path placement, fonts, controls and the reading/storage engine are unchanged. Controls and Credits remain accessible. See [release notes and known limitations](RELEASE_NOTES.md). This is the V1.1 final release; physical Supercard/SD hardware remains unverified.

## Retained v0.8.0 changes

- Saved EPUB opening validates ZIP/package metadata, source identity and a small checksum index rather than scanning all cached text. Each 4 KiB text block is verified before use; the restored page does not require reading earlier blocks.
- A retained checksum window avoids repeated seeks to the index while paging. Fixed-block CRC combination uses a precomputed read-only operator instead of rebuilding it for each block.
- A bounded central-directory lookup accelerator, combined fingerprint pass and bulk metadata reads reduce repeated directory scans and seeks. Hash collisions fall back to exact duplicate-detecting lookup; required ZIP safety checks remain.
- Smart single quotes `‘ ’ ‚` display as `'`, smart double quotes `“ ” „` as `"`, and typographic hyphens `‐ ‑` as `-`, matching gbawriter's plain punctuation. This changes display only, not source bytes or normalized cache text. Ellipses, en/em dashes, guillemets and other symbols stay unchanged.
- Library `Start` opens credits; `B` or `Start` closes them. The library shows `start: credits` near the bottom.
- A checksummed display-layout marker preserves current-layout Back history on reopen while rebuilding older, incompatible page boundaries once. Saved byte positions and settings remain intact.

**Upgrade:** v0.6/v0.7 owned caches remain readable with their old full-text checks. Open the book and press `Start` to save once, then close and reopen it to use the new block cache. The internal cache format is now 6; ZIP member names remain `cache-v5` and `state-v5`. Further saves in the same session remain state-only rather than repeatedly rebuilding the cache.

**Integrity policy:** cache headers, the checksum index, source fingerprint, ZIP/package structure and required-entry bounds are checked upfront. Unread cached-text corruption is detected when its block is needed. A failed block is never displayed: the reader falls back to validated originals, or reports an error if they cannot safely supply the text. The next save can replace a damaged cache. Original chapter payload corruption that leaves ZIP metadata unchanged remains deferred until original chapters are needed. CRCs detect accidental damage, not malicious coordinated checksum replacement. Original EPUB entries remain available to other readers; no permanent sidecars are introduced.

## Retained v0.6.0 improvements

- ZIP filename validation avoids alternating distant reads for each character.
- Cache preparation exports normalized text sequentially with bounded buffers and calculates checksums without rereading the normalized book.
- Invalid-cache fallback retains original-content CRC and complete DEFLATE-stream validation. The current cache-hit policy is described above.
- Newly appended ZIP directories contain only the latest bookmark state. Invalid-cache regeneration replaces the old live cache entry without deleting unknown entries.
- Transient EPUB save errors reacquire the FatFS handle before rollback, including legacy migration. Existing original bytes are not overwritten by EPUB appends.
- Unchanged settings preserve Back history, intermediate settings edits do not paginate a hidden preview, and pagination checks line fit before processing the line.
- CI runs host, sanitizer and real-FatFS regression tests plus the ROM build on main and pull requests.

Physical compaction is deferred: files still grow with saves, and saves are rejected before exceeding the reader's archive-size limit. Cache/state member names remain v5 for compatibility.

## Features

- Case-insensitive `.txt` and `.epub` list directly on Home, restricted to `/gbareader` on the SD-card root; no card-wide browser or demo fallback
- EPUB discovery uses FAT long filenames up to 255 bytes and indexes up to 64 books
- Buffered FatFS access; books are streamed instead of loaded into GBA RAM
- Direct EPUB ZIP reading with stored and raw-DEFLATE entries; processed required entries receive CRC-32 verification, while valid-cache opens use the integrity policy above; nothing is extracted to SD
- EPUB container, OPF manifest and declared spine-order handling
- URI percent-decoding and XML entity decoding for container, manifest and spine references
- Preferred EPUB package selection when `container.xml` declares multiple rootfiles
- Compact indexing for up to 256 readable spine documents and 255-byte internal archive paths
- Recognized image archive members are skipped before local-header or payload processing; image-only spine entries are omitted from the text stream
- XHTML visible-text conversion with CDATA, semantic block/table breaks and common XML/HTML entities
- Word wrapping and CRLF/LF handling
- Fixed native 16-pixel SuperFW body-font size
- SuperFW font coverage for supported Latin, Greek, Cyrillic, Japanese, CJK and Hangul text
- Compact supplemental publishing-symbol font covering smart quotes, dashes, ellipsis, bullets, `€`, `™` and related punctuation/currency glyphs
- Display-only ASCII forms for smart quotes, typographic hyphens, Unicode minus and full-width ASCII forms
- Dedicated `gba-vocab-trainer-CC` UI font for menus
- Line spacing, top margin and bottom margin settings, each adjustable from 1 through 4
- Fixed 64-page circular Back history, persisted in current-format bookmarks
- Checksummed embedded save state preserves reading position, settings and up to 64 previous page offsets without sidecars
- The first successful EPUB save writes normalized text and state as stored `META-INF/gbareader/*-v5` ZIP members, retaining ordinary EPUB/ZIP validity
- Later EPUB saves append a replacement v5 state member. TXT footer replacement attempts to restore the previous footer and file length on failure; persistent write failure or interruption can lose the bookmark even where original body text is preserved
- Saved page history makes Back immediate after reopening a current-format bookmark; older bookmarks rebuild Back history incrementally without a blocking full-book scan
- Save completion is reported as `Saved` or `Save failed`, then disappears automatically while controls remain responsive
- Runs of three or more spaces collapse to one space, and repeated newlines collapse to one line break
- UTF-8 BOM handling and safe replacement of malformed or unsupported input
- White reading page with black text

## Explicit scope

- **Arabic is not supported.** Arabic code points are rendered as `?`; no shaping or bidirectional path is included.
- **Text size is fixed.** The reader keeps the native 16-pixel SuperFW bitmap body font.
- **This is not full EPUB compliance.** Images are skipped completely, including image-only spine pages. CSS presentation, JavaScript, embedded fonts, audio, video, SVG presentation and DRM are unsupported and ignored or rejected as appropriate. Arabic is unsupported.
- TXT uses a trailing save footer. EPUB v0.5 state and cache are ZIP members, not post-EOCD data. A bounded detector recognizes a valid v0.4.7 cache trailer plus v1/v2 footer, exposes its original pre-trailer archive and bookmark, and rewrites it as v0.5 ZIP members on the next successful save.
- ZIP64 and multi-disk archives are rejected. Required metadata and spine text must be unencrypted and use stored (0) or DEFLATE (8) compression; unsupported methods or encryption on recognized, ignored image assets do not prevent reading.
- The ZIP central directory is validated as a stream, so image-heavy EPUBs are not rejected merely for containing more than 128 archive members. Remaining compile-time limits are 256 readable spine documents, 255-byte archive paths, 64 KiB uncompressed metadata, 32 MiB uncompressed XHTML per spine document, 16 MiB compressed required entry and 128 MiB archive. XHTML is inflated and converted through a 32 KiB dictionary and 16 KiB visible-text window, never a whole-chapter allocation. Ignored images, fonts and other non-spine assets are not subject to the XHTML limit. Each applicable limit has a distinct user-facing error; text is never silently truncated.
- EPUB package metadata and XHTML must be UTF-8. UTF-16 XML/XHTML is outside this release's bounded parser scope.

## Controls

Read TXT and EPUB books, save your reading position, and resume later. Place UTF-8 `.txt` and supported `.epub` files directly in `/gbareader` on the SD-card root, then restart the app. It lists up to 64 files, not subfolders, with five books visible at once. A missing or empty folder shows instructions; Controls and Credits remain available. This reader has no text editing, typing or user-facing export controls.

See [the full controls PDF](docs/gbareader-full-controls.pdf).

### Library

- `Up` / `Down`: select a `.txt` or `.epub` book
- `A`: open the selected book
- `Select`: show Controls. `Left` / `Right` changes help pages; `B` returns Home.
- `Start`: show credits; `B` or `Start` returns to the library

### Reader

- `Right` or `A`: next page
- `Left` or `B`: previous page
- `Down`: open the reader settings
- `Up`: enable or disable shoulder-button page turns for the current session. When enabled, `L` goes to the previous page and `R` goes to the next page. This option starts disabled whenever the app launches and is not saved.
- `Start`: save the current page, reader settings and up to 64 previous page offsets inside the open TXT or EPUB file. A `save...` message appears while writing, followed temporarily by the honest result: `Saved` or `Save failed`.
- `Select`: close the book and return to the library without saving

Current-display-layout bookmarks retain up to 64 previous page offsets. Older or unknown display-layout bookmarks keep their saved byte position but rebuild Back history incrementally, because their glyph widths may differ. An early Back request shows `Loading back...` while reconstruction proceeds.

### Settings

- `Up` / `Down`: select line spacing, top margin or bottom margin
- `Left` / `Right`: change the selected value
- `B` or `Start`: apply the displayed settings and return to the reader. To retain them in either TXT or EPUB, press `Start` again from the reader. Font size is fixed at 16 pixels.

## Hardware and files

The app uses the Supercard SD access path inherited from the base engine. Create a **gbareader** folder at the SD-card root and copy UTF-8 `.txt` or supported `.epub` files directly into it. Home indexes up to 64 files and retains filenames up to 255 bytes for opening; long displayed names are clipped by pixel width without changing their bytes. Embedded save state retains the current position and reading-layout settings for both formats. No `.sav` or permanent settings sidecar is used. Initial EPUB cache preparation writes initial state automatically. Later position, Back history and settings changes require a successful Reader `Start` save before leaving with `Select`.

An emulator without the expected Supercard storage interface can validate the ROM header and execute the UI path, but it cannot prove SD/FatFS behavior. Real-hardware verification remains important.

## Building

Requirements:

- devkitPro/devkitARM
- Butano
- GNU Make
- A Python 3 interpreter used by Butano's asset tools

```sh
make clean LIBBUTANO=/absolute/path/to/butano/butano
make -j2 LIBBUTANO=/absolute/path/to/butano/butano
```

The ROM is written to `gbareader.gba`.

### Host tests

```sh
make host-test LIBBUTANO=/absolute/path/to/butano/butano
```

### Real FatFS save-recovery tests

Install GCC/G++, Python 3, `dosfstools` and `mtools`, then run:

```sh
python3 tests/fatfs/run.py
GBAREADER_FATFS_FIXTURE=legacy-v047.epub python3 tests/fatfs/run.py
```

These exercise the production FatFS branch on images and gate successful saves and exercised transient-error recovery. See [the harness documentation](tests/fatfs/README.md) for evidence paths and diagnostic limits. Persistent errors and arbitrary power loss are not guaranteed recoverable; physical Supercard testing remains separate.

### Optional emulator smoke test

```sh
make test LIBBUTANO=/absolute/path/to/butano/butano
```

The inherited emulator target is not sufficient release evidence: it can report a pass after a frontend/display startup failure. Verify an actual visible application screen independently. Neither this target nor an emulator without Supercard support proves SD/FatFS hardware behavior.

## Architecture

- `reader_core`: host-testable UTF-8 decoding, wrapping, pagination and page history
- `reader_file`: FatFS library scan, 8-KiB read window, bounded sequential ZIP cache/state appends, transient-error rollback and replaceable trailing TXT save-footer I/O
- `epub_document`: bounded ZIP/container/OPF parser plus stored/DEFLATE XHTML-to-text conversion, exposed as a virtual concatenated `ByteSource`; uncached open establishes stable text offsets, sequential export retains per-chapter parser/inflater state, and cached open validates package metadata and the checksum index before verifying normalized text blocks on demand without inflating original spine chapters
- `reader_txt_save`: checksummed, versioned embedded save-footer encoding retained compatibly from TXT support
- `main`: Butano UI, controls and page presentation
- `superfw_font`: SuperFW software glyph renderer targeting a double-buffered 8-bit bitmap background

Body text is rendered into a bitmap page rather than creating one GBA sprite per glyph. This avoids the 128-object OAM limit that makes a full-page sprite-text reader impractical. The menu UI remains sprite-based.

## Provenance and licensing

The project was derived from the exact `gba-vocab-trainer-CC` `v0.2.5` tagged source. Its Supercard/FatFS integration and customized UI-font foundation are retained. SuperFW font-rendering sources and `fonts.pack` are included under `references/superfw/` with their upstream notices.

The tinfl-only miniz files are vendored from commit `77d0dce8627735138c51770d1799a1ef48f2117d`, configured without allocation, compression, zlib, stdio, time or miniz archive APIs. Provenance and its MIT license are in `third_party/miniz/`.

The inherited project and SuperFW components are distributed under the GNU General Public License, version 3 or later. See [`LICENSE`](LICENSE).

## Possible later work

- Directory navigation and larger libraries
- Multiple bookmarks per book
- Broader EPUB compatibility within the GBA memory budget
