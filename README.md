# GBA Reader

A focused plain-text e-reader for the Game Boy Advance, built from the proven SD/FatFS and font foundation of [`gba-vocab-trainer-CC` v0.2.5](https://github.com/Xinon232/gba-vocab-trainer-CC/releases/tag/v0.2.5).

Version 0.4.7 opens UTF-8 `.txt` and a deliberately bounded, text-only subset of UTF-8 EPUB 2/3 files directly from a Supercard SD card.

## v0.4.7 features

- Case-insensitive `.txt` and `.epub` browser for files in the SD-card root
- EPUB discovery uses FAT long filenames up to 255 bytes and indexes up to 64 books
- Buffered FatFS access; books are streamed instead of loaded into GBA RAM
- Direct EPUB ZIP reading with stored and raw-DEFLATE entries and required-entry CRC-32 verification; nothing is extracted to SD
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
- Readable ASCII fallback for Unicode minus and full-width ASCII forms
- Dedicated `gba-vocab-trainer-CC` UI font for menus
- Line spacing, top margin and bottom margin settings, each adjustable from 1 through 4
- Fixed 64-page circular Back history, persisted in current-format bookmarks
- Checksummed embedded TXT/EPUB save footer preserves reading position, settings and up to 64 previous page offsets without sidecars; repeat saves replace the footer instead of growing the book
- The first successful EPUB save appends a normalized-text cache plus a fresh copy of the ZIP central directory/end record without rewriting any original bytes; later opens verify the compact text sequentially, then use direct cached reads instead of reparsing and inflating every chapter
- Later EPUB saves replace only the bookmark footer, preserving the cache without further file growth; interrupted first-cache writes restore the exact prior footer/file length
- Saved page history makes Back immediate after reopening a current-format bookmark; older bookmarks rebuild Back history incrementally without a blocking full-book scan
- Save completion is reported as `Saved` or `Save failed`, then disappears automatically while controls remain responsive
- Runs of three or more spaces collapse to one space, and repeated newlines collapse to one line break
- UTF-8 BOM handling and safe replacement of malformed or unsupported input
- White reading page with black text

## Explicit scope

- **Arabic is not supported.** Arabic code points are rendered as `?`; no shaping or bidirectional path is included.
- **Text size is fixed.** v0.4.2 keeps the native 16-pixel SuperFW bitmap body font.
- **This is not full EPUB compliance.** Images are skipped completely, including image-only spine pages. CSS presentation, JavaScript, embedded fonts, audio, video, SVG presentation and DRM are unsupported and ignored or rejected as appropriate. Arabic is unsupported.
- TXT and EPUB books can be saved manually with embedded trailing application data. Original EPUB bytes are never rewritten. On the first successful EPUB save, GBAReader appends normalized text, a duplicate central directory and fresh ZIP end record, then a compact cache trailer and bookmark footer. Keeping a valid ZIP end near the file end preserves compatibility with ordinary EPUB/ZIP readers even for caches larger than 64 KiB.
- ZIP64 and multi-disk archives are rejected. Required metadata and spine text must be unencrypted and use stored (0) or DEFLATE (8) compression; unsupported methods or encryption on recognized, ignored image assets do not prevent reading.
- The ZIP central directory is validated as a stream, so image-heavy EPUBs are not rejected merely for containing more than 128 archive members. Remaining compile-time limits are 256 readable spine documents, 255-byte archive paths, 64 KiB uncompressed metadata, 32 MiB uncompressed XHTML per spine document, 16 MiB compressed required entry and 128 MiB archive. XHTML is inflated and converted through a 32 KiB dictionary and 16 KiB visible-text window, never a whole-chapter allocation. Ignored images, fonts and other non-spine assets are not subject to the XHTML limit. Each applicable limit has a distinct user-facing error; text is never silently truncated.
- EPUB package metadata and XHTML must be UTF-8. UTF-16 XML/XHTML is outside this release's bounded parser scope.

## Controls

### Library

- `Up` / `Down`: select a `.txt` or `.epub` book
- `A`: open the selected book

### Reader

- `Right` or `A`: next page
- `Left` or `B`: previous page
- `Down`: open the reader settings
- `Up`: enable or disable shoulder-button page turns for the current session. When enabled, `L` goes to the previous page and `R` goes to the next page. This option starts disabled whenever the app launches and is not saved.
- `Start`: save the current page, reader settings and up to 64 previous page offsets inside the open TXT or EPUB file. A `save...` message appears while writing, followed temporarily by the honest result: `Saved` or `Save failed`.
- `Select`: close the book and return to the library without saving

When opening a v0.4.7 bookmark, up to 64 saved previous pages are immediately available. Older bookmarks are still accepted; their Back history is rebuilt incrementally, and an early Back request shows `Loading back...` without blocking the application.

### Settings

- `Up` / `Down`: select line spacing, top margin or bottom margin
- `Left` / `Right`: change the selected value
- `B` or `Start`: apply the displayed settings and return to the reader. To retain them for a TXT book, press `Start` again from the reader.

## Hardware and files

v0.4.7 uses the Supercard SD access path inherited from the base engine. Copy UTF-8 `.txt` or supported `.epub` files to the **root** of the SD card. The browser indexes up to 64 files and retains filenames up to 255 bytes for opening. Embedded save footers retain the current position and reading-layout settings for both formats.

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

### Optional emulator smoke test

```sh
make test LIBBUTANO=/absolute/path/to/butano/butano
```

The emulator target only checks for immediate ROM/header or illegal-opcode rejection; it does not claim successful Supercard SD behavior.

## Architecture

- `reader_core`: host-testable UTF-8 decoding, wrapping, pagination and page history
- `reader_file`: FatFS library scan, 512-byte cached logical book stream, transactional appended EPUB text cache and replaceable trailing save-footer I/O
- `epub_document`: bounded ZIP/container/OPF parser plus streaming stored/DEFLATE XHTML-to-text conversion, exposed as a virtual concatenated `ByteSource`; uncached open performs a count-only pass for stable offsets, while cached open CRC-validates and directly maps the appended normalized text
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
