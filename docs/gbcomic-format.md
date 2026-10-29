# The .gbcomic format (version 2)

A `.gbcomic` file holds a comic as ready pictures for gbareader on the Game
Boy Advance: each page scaled for each zoom and screen angle. It is made by
the gbcomic conversion software ([halimj.itch.io](https://halimj.itch.io/)) and read by `src/gbcomic.cpp`. Everything that costs
time is done by the converter: the GBA only reads pieces of the file and
unpacks them. Where the screens go on a picture is the GBA's choice
(`src/comic_layout.cpp`), so the reader can change it.

All numbers are little-endian. Offsets are from the start of the file.
Every page record, view and picture starts on a 4-byte boundary.

## Header (32 + 4 × zooms bytes)

| At | Size | |
|---|---|---|
| 0 | 8 | `GBCOMIC1` |
| 8 | 2 | version: 2 |
| 10 | 2 | page count (1..65535) |
| 12 | 1 | angles: bit 0 upright (240×160 screens), bit 1 sideways (160×240) |
| 13 | 1 | zoom count (1..48; the converter writes at most 44: Fit page, Fit width and 1.1x to 3.0x for both angles; readers before the 48-zoom build take 16) |
| 14 | 2 | header size: 32 + 4 × zoom count |
| 16 | 4 | page table offset |
| 20 | 4 | file size (a shorter or longer file is refused) |
| 24 | 8 | zero |
| 32 | 4 × zooms | the zooms: angle (1 byte: 0 upright, 1 sideways), kind (1: 0 Fit page, 1 a width), factor (2: the picture's width in hundredths of the screen width; 100 is Fit width; 0 for Fit page) |

The zooms of each angle are listed smallest first. The converter makes Fit
page and Fit width for each angle it makes, then the zooms asked for
(by default 1.6x, 1.8x, 2x upright and 2.2x, 2.4x, 2.6x sideways).

## Page table

`page count + 1` offsets (4 bytes each): page *i* is the record at entry
*i*; the last entry is the end of the last page.

## Page record

| Size | |
|---|---|
| 2 | view count |
| 2 | zero |
| 2, 2 | the original page's width and height in pixels (for the page map) |
| 8 × views | view entries: zoom (1 byte: its number in the header's list), zero (1), picture count (2), view offset (4) |

## View

A view is one zoom of a page: one picture (Fit width and the zooms) or one
per half of a spread (Fit page).

| Size | |
|---|---|
| 3 × pictures | picture order for each reading order: left to right, right to left, columns (picture numbers) |
| (to 4 bytes) | |
| 4 × pictures | picture offsets |

## Picture

| Size | |
|---|---|
| 2, 2 | width, height |
| 2 | strip width (half a screen: 120 upright, 80 sideways) |
| 2 | strip count (`ceil(width / strip width)`) |
| 2, 2 | ink grid width and height (`ceil(width / 8)`, `ceil(height / 8)`) |
| 4 × 2 | where the picture is on the page: left, top, right, bottom in 1/4096 of the page |
| 2 | rows per chunk (16) |
| 2 | zero |
| ink bytes | one bit per 8×8 pixels, row by row, lowest bit first: 1 where anything but paper is (`ceil(grid width × grid height / 8)` bytes) |
| (to 4 bytes) | |
| 248 × 2 | colours: BGR555, for screen colours 8..255 |
| 4 × (strips × chunks + 1) | chunk offsets, strip by strip, top to bottom; the last is the end of the last chunk |
| … | chunks |

The reader places the screens itself: as many as cover the picture (or as
many as the reader asks for), spread evenly, moved where the reader points
them. The ink grid tells it which screens hold nothing but paper.

Pixels are screen colour indices (8..255); 0 is the reader's paper white,
used outside the picture. They are stored in vertical strips, each `rows
per chunk` rows of a strip forming one chunk (`strip width × rows` bytes,
less at the right and bottom edges), packed in the GBA BIOS LZ77 format
(type `0x10`: a 4-byte header with the unpacked size, then flag bytes for
8 literals or 2-byte references of 3..18 bytes up to 4096 back). The chunks
of one strip are in order, so the chunks for one screen are one read.

## Reading a part of a picture

1. Page table, page record, view entry, the picture order and offsets, and
   each picture's head and ink grid.
2. For a rectangle of a picture: its colours; for each strip it covers, the
   chunk offsets for its rows and those chunks in one read; unpack each
   chunk and copy its part.

## Compatibility

A reader refuses a file whose version is higher than it knows (`Made for a
newer gbareader`). Version 1 files (screens placed by the converter, a fixed
set of zooms) are refused too (`Old .gbcomic: convert again`). Fields marked zero must be written as zero; a later
version may use them.
