# GBA Reader

A focused plain-text e-reader for the Game Boy Advance, built from the proven SD/FatFS and font foundation of [`gba-vocab-trainer-CC` v0.2.5](https://github.com/Xinon232/gba-vocab-trainer-CC/releases/tag/v0.2.5).

Version 0.2.2 opens UTF-8 `.txt` and a deliberately bounded, text-only subset of `.epub` files directly from a Supercard SD card.

## v0.2.2 features

- Read-only, case-insensitive `.txt` and `.epub` browser for files in the SD-card root
- EPUB discovery uses FAT long filenames up to 255 bytes and indexes up to 64 books
- Buffered FatFS access; books are streamed instead of loaded into GBA RAM
- Direct EPUB ZIP reading with stored and raw-DEFLATE entries and required-entry CRC-32 verification; nothing is extracted to SD
- EPUB container, OPF manifest and declared spine-order handling
- Recognized image archive members are skipped before local-header or payload processing; image-only spine entries are omitted from the text stream
- XHTML visible-text conversion with block breaks and common XML/HTML entities
- Word wrapping and CRLF/LF handling
- Fixed native 16-pixel SuperFW body-font size
- SuperFW font coverage for supported Latin, Greek, Cyrillic, Japanese, CJK and Hangul text
- Dedicated `gba-vocab-trainer-CC` UI font for menus
- Line spacing, top margin and bottom margin settings, each adjustable from 1 through 4
- Page-forward and page-back history
- Checksummed SRAM persistence for settings and independent positions for up to 32 filenames
- Automatic position saves when a book opens, a page changes, settings change or the reader closes; `R` saves manually
- Runs of three or more spaces collapse to one space, and repeated newlines collapse to one line break
- UTF-8 BOM handling and safe replacement of malformed or unsupported input
- White reading page with black text

## Explicit scope

- **Arabic is not supported.** Arabic code points are rendered as `?`; no shaping or bidirectional path is included.
- **Text size is fixed.** v0.2.2 keeps the native 16-pixel SuperFW bitmap body font.
- **This is not full EPUB compliance.** Images are skipped completely, including image-only spine pages. CSS presentation, JavaScript, embedded fonts, audio, video, SVG presentation and DRM are unsupported and ignored or rejected as appropriate. Arabic is unsupported.
- Books are read-only; the application never rewrites source files or extracts archive members to SD.
- ZIP64 and multi-disk archives are rejected. Required metadata and spine text must be unencrypted and use stored (0) or DEFLATE (8) compression; unsupported methods or encryption on recognized, ignored image assets do not prevent reading.
- The ZIP central directory is validated as a stream, so image-heavy EPUBs are not rejected merely for containing more than 128 archive members. Remaining compile-time limits are 64 spine documents, 192-byte archive paths, 64 KiB uncompressed input/visible text per required chapter or metadata file, 4 MiB compressed required entry and 64 MiB archive. Ignored images, fonts and other non-spine assets are not subject to the 64 KiB text-buffer limit. Exceeding an applicable limit is an error; text is never silently truncated.

## Controls

### Library

- `Up` / `Down`: select a `.txt` or `.epub` file
- `A`: open selected file

### Reader

- `Right` or `A`: next page
- `Left` or `B`: previous page
- `Start`: settings
- `R`: save the current position manually
- `Select`: return to library

### Settings

- `Up` / `Down`: select setting
- `Left` / `Right`: change value
- `B` or `Start`: save and return to reading

## Hardware and files

v0.2.2 uses the Supercard SD access path inherited from the base engine. Copy UTF-8 `.txt` or supported `.epub` files to the **root** of the SD card. The browser indexes up to 64 files and retains filenames up to 255 bytes for opening. SRAM retains independent positions for up to 32 filenames; older v0.1.0/v0.2.0/v0.2.1 saves migrate automatically.

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
- `reader_file`: read-only FatFS library scan and 512-byte cached book stream
- `epub_document`: bounded ZIP/container/OPF/XHTML parser exposed as a virtual concatenated `ByteSource`; one chapter is cached at a time
- `reader_save`: checksummed, versioned SRAM state
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
