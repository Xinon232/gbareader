# GBA Reader

A focused plain-text e-reader for the Game Boy Advance, built from the proven SD/FatFS and font foundation of [`gba-vocab-trainer-CC` v0.2.5](https://github.com/Xinon232/gba-vocab-trainer-CC/releases/tag/v0.2.5).

The first release is intentionally small: open UTF-8 `.txt` files from a Supercard SD card, read them page by page, adjust page spacing, and resume where you stopped.

## v0.1.0 features

- Read-only `.txt` browser for files in the SD-card root
- Buffered FatFS access; books are streamed instead of loaded into GBA RAM
- Word wrapping and CRLF/LF handling
- Fixed native 16-pixel SuperFW body-font size
- SuperFW font coverage for supported Latin, Greek, Cyrillic, Japanese, CJK and Hangul text
- Dedicated `gba-vocab-trainer-CC` UI font for menus
- Adjustable line spacing, top margin and bottom margin
- Page-forward and page-back history
- SRAM persistence for settings, last book and source byte offset
- UTF-8 BOM handling and safe replacement of malformed or unsupported input
- White reading page with black text

## Explicit scope

- **Arabic is not supported.** Arabic code points are rendered as `?`; no shaping or bidirectional path is included.
- **Text size is fixed.** v0.1.0 deliberately keeps the native SuperFW font size.
- EPUB is not supported yet.
- Books are read-only; the application never rewrites source `.txt` files.

## Controls

### Library

- `Up` / `Down`: select a `.txt` file
- `A`: open selected file

### Reader

- `Right` or `A`: next page
- `Left` or `B`: previous page
- `Start`: settings
- `Select`: return to library

### Settings

- `Up` / `Down`: select setting
- `Left` / `Right`: change value
- `B` or `Start`: save and return to reading

## Hardware and files

v0.1.0 uses the Supercard SD access path inherited from the base engine. Copy UTF-8 `.txt` files to the **root** of the SD card. The browser currently indexes up to 32 files and stores filenames up to 63 bytes for display/opening.

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
- `reader_save`: checksummed, versioned SRAM state
- `main`: Butano UI, controls and page presentation
- `superfw_font`: SuperFW software glyph renderer targeting a double-buffered 8-bit bitmap background

Body text is rendered into a bitmap page rather than creating one GBA sprite per glyph. This avoids the 128-object OAM limit that makes a full-page sprite-text reader impractical. The menu UI remains sprite-based.

## Provenance and licensing

The project was derived from the exact `gba-vocab-trainer-CC` `v0.2.5` tagged source. Its Supercard/FatFS integration and customized UI-font foundation are retained. SuperFW font-rendering sources and `fonts.pack` are included under `references/superfw/` with their upstream notices.

The inherited project and SuperFW components are distributed under the GNU General Public License, version 3 or later. See [`LICENSE`](LICENSE).

## Planned later work

- Directory navigation and larger libraries
- Bookmarks and per-book position records
- EPUB ingestion after the TXT reader is stable
