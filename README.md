# gbareader V3.1

Read TXT and EPUB books on your Game Boy Advance. Save your place and return to it later. Put books directly in `/gbareader` on a compatible Supercard SD card; they appear on Home.

## V3.1: gbamp3 look, Import, Settings from Select

Every screen except the book page now looks like gbamp3 v1.8: white background, a small centred header, eight 17-pixel rows and a light-blue bar on the selected row. Long names scroll on the selected row. No instructions stay on screen; the most important controls are on About > Controls.

- Home is the book list at once, with the cursor on the last opened book. Up/Down move (a fresh press wraps, holding repeats), Left/Right move a page, A opens.
- `Select` opens Settings, on Home and while reading. While reading its first row is `Back to Files`, which closes the book (as Select did before). If the page shown is not the saved one, that row first turns into `Continue without saving?`: `A` leaves, `B` (or moving the cursor) brings back `Back to Files`. Settings is the only way back to the file list.
- `Start` on Home opens the import browser (`Import to /gbareader`): folders from the SD root and the TXT/EPUB books in them. Choosing a book asks `Import into /gbareader?` (No first); Yes copies it into `/gbareader` (created if missing) and returns to the refreshed list with the new book selected. An existing file of the same name is never overwritten.
- `L/R page turns` is a Settings row and is remembered: the last choice, from Settings or from holding Up while reading, is saved at once and restored at launch. The old `L/R on startup` option is gone (its saved value becomes the starting choice).
- Holding Down while reading no longer does anything. Reading itself (page turns, Start saves, overlays, layout) is unchanged.
- About (Settings > About, Left/Right): About, Controls, License, Text and fonts, Arabic font, SD card and files, EPUB and engine. Contact: halimj.itch.io, gba@halim-jarrar.de.

## Preserved V3.0: global settings and last-book selection

Line spacing, paragraph gap and `L/R on startup` are now shared across books and saved when Settings closes. Home starts with the last successfully opened book selected (never auto-opened), or the first book if that exact filename is missing. Two small checked files, `/gbareader/SETTINGS0.DAT` and `SETTINGS1.DAT`, hold these globals independently of each book's position/Arabic/cache. (V3.1 replaces `L/R on startup` with the remembered `L/R page turns`.)

## V1.9: no limit on the number of books

Home lists every supported book in `/gbareader`, no longer only the first 64. Names are read from the folder 64 at a time as you move through the list, so RAM use is unchanged; moving past a block of names rereads the folder once. Books keep folder order. Everything else is as in V1.8.

## Preserved V1.8: faster opening of EPUBs

Reopening an EPUB that already has its `.sav` cache no longer re-checks the whole ZIP file or rebuilds the chapter list: the companion already proves the book is unchanged, so only the small cache table is read (in tests with a 1.9 MB, 200-chapter book, the EPUB step went from 668 disk reads / 9,728 sectors to 6 / 6). The full checks still run when there is no valid cache, and if a cached block ever fails its checksum the original chapters are checked and used as before. The first open of a new EPUB also reads much less: isolated header lookups read one 512-byte sector instead of refilling an 8 KiB window (636 disk reads / 7,146 sectors instead of 1,148 / 17,498 for the same book). Reading, storage and controls are unchanged.

## Preserved V1.7: quick Back after Go to, page numbers, gbamp3 UI font

Back right after a Go to (or a resume deep in a book) no longer waits while the whole book up to that point is laid out: Back history is rebuilt from a nearby line start and the next older pages load ahead. The page number keeps counting while Settings is open, shows an estimate (`Page about 142`) until the exact number is known, and Go to keeps the pages already counted. UI text now uses the 5x7 font from gbamp3; the blue `>` cursor also marks the selected book on Home. Book text, storage and controls are unchanged.

## Preserved V1.6: reader settings that change the page

Top and bottom margin are replaced by settings that change how much text fits. Line spacing now goes from 0 to 4 (0 fits ten lines), and the new paragraph gap (None, Small, Half or Full line) controls the space after each paragraph. Text is centered vertically, and settings show lines per page, the current page number and percentage, and a Go to percentage that jumps when settings close. Defaults (spacing 1, Full gap) keep nine lines and the previous look. All V1.5 controls and V1.4 storage are preserved.

## Preserved V1.5: deliberate reader controls

Reader Up/Down now require the same continuous solo hold as gbawriter Caps. A brief foreground UI-font notice confirms shoulder page-turn mode. All V1.4 storage, rendering and other controls are preserved.

## Preserved V1.4: unchanged books, individual SAV companions

All newly generated per-book data now lives in one companion alongside each book: `Title.txt.sav` for `Title.txt`, and `Title.epub.sav` for `Title.epub`. The original extension is retained, so the two formats do not collide. **The app opens books read-only and never writes, strips, truncates or replaces their bytes**, including during migration and failed saves.

A companion holds the byte bookmark, sticky Arabic ON/OFF, compatible 64-entry Back history/rebuild metadata, and the entire normalized EPUB text cache. Saved spacing/gap remain only as history-layout metadata; they never override global preferences. Two fixed-size checked state banks are updated alternately. The large cache is separate: ordinary bookmarks neither grow a log nor rewrite that cache. TXT companions occupy at most 2048 bytes. EPUB companions use 2048 bytes plus a 32-byte cache header, normalized text and a four-byte checksum per 4096-byte block, capped at 128 MiB total.

The V1.4 storage design preserved page layout, fonts, shaping, input and ordinary page-turn behavior. It preserves the existing lazy cache policy: ZIP/package/spine guards, source fingerprint, header and checksum table are checked on opening; normalized blocks are checked as needed. Original chapter inflation/CRC is deferred on a valid cache hit. A failed block is not displayed: validated originals are used, or reading fails safely. New cache creation includes sequential saved-payload readback. No physical SD speed claim is made.

V3.1 publication is separate from updating `main`; unrelated historical releases remain unchanged. Technical details: [SAV format, limits and recovery](docs/sidecar-format.md).

## Features and bounds

- Case-insensitive UTF-8 `.txt` and a bounded text-only subset of EPUB 2/3; any number of files directly in `/gbareader` (names are read from the folder 64 at a time), eight visible at once. Subfolders of `/gbareader` are not read; the import browser copies books from anywhere on the card. No demo book.
- Filenames remain intact and are clipped only on screen. Discovery accepts up to 255 UTF-8 bytes; saving needs room for `.sav`, so the full book filename must be at most 251 UTF-8 bytes. Longer names remain readable but cannot be saved until renamed on a computer.
- Streaming FatFS reads through an 8 KiB window; no whole-book RAM allocation.
- Stored/raw-DEFLATE ZIP entries, container/OPF manifest and declared spine order; URI percent-decoding and XML entities in references; preferred package selection for multiple rootfiles.
- Visible XHTML text including CDATA, block/table breaks and common XML/HTML entities. Images and image-only spine pages are skipped.
- Limits remain 256 readable spine documents, 255-byte internal paths, 64 KiB uncompressed metadata, 32 MiB uncompressed XHTML per spine document, 16 MiB compressed required entry and 128 MiB source archive. A 32 KiB inflater dictionary and 16 KiB text window bound chapter processing. Applicable errors are reported, never silently truncated.
- ZIP64, multi-disk archives, encrypted required entries and unsupported required compression are rejected. UTF-16 XML/XHTML, DRM, CSS presentation, scripts, embedded fonts, audio, video and SVG presentation are outside scope.
- Native 16px SuperFW body font, the gbamp3 5x7 UI font, supported Latin/Greek/Cyrillic/Japanese/CJK/Hangul and supplemental publishing symbols. Smart quotes and selected hyphens use the existing display-only ASCII substitutes.
- Word wrapping, CRLF/LF, UTF-8 BOM and safe malformed-input replacement. Runs of three or more spaces collapse on screen; repeated newlines collapse to one line break. None of this edits the source.
- Line spacing from 0 to 4 and paragraph gap None/Small/Half/Full, with text centered vertically; page number, percentage and Go to percentage in settings; byte anchors rather than saved page numbers; incremental Back-history rebuilding when layout markers or effective spacing/gap/Arabic differ.
- Native Ghoulam Arabic joining/lam-alef and limited per-line RTL. Harakat are hidden only on screen. Latin and digit runs remain left-to-right. This is not full Unicode bidi or full Persian/Urdu support.

## Controls

Read TXT and EPUB books, save your reading position, and resume later. Place UTF-8 `.txt` and supported `.epub` files directly in `/gbareader` on the SD-card root, then restart the app, or import them with `Start`. It lists every supported file (no limit), not subfolders, with eight books visible at once. A missing or empty folder shows `No books found`; `Start` imports books from anywhere on the card. This reader has no text editing, typing or user-facing export controls.

The user manual with screenshots, credits and licenses: [PDF](docs/gbareader-full-controls.pdf) / [Markdown](docs/gbareader-full-controls.md). This section is the technical reference.

### Home (file list)

- `Up` / `Down`: move (a fresh press on the first/last book wraps; holding repeats and stops at the ends). `Left` / `Right`: one page of eight books.
- `A`: open the selected book. Each successful open remembers its full filename; on next launch Home selects it again, without opening it. Missing or renamed books fall back to the first entry.
- `Select`: Settings. `Start`: import browser.

### Import

- Lists the folders (with a folder icon, first) and TXT/EPUB books of the current folder, starting at the SD root. Hidden/system entries and `/gbareader` itself are not shown.
- `A` on a folder opens it; `B` goes up one folder (from the root: back Home).
- `A` on a book asks `Import into /gbareader?` with `No` selected. `Yes` copies the book (progress in percent) and shows Home with the new book selected and `Imported`. `Already in /gbareader` means a file with that name exists there; it is left unchanged. A failed copy leaves no partial file.

### Reader

- `Right` or `A`: next page. `Left` or `B`: previous page.
- Hold `Up` alone (about 0.8 s): toggle shoulder page turns (`L` back, `R` forward). `L+R: On` / `L+R: Off` appears for about a second; the choice is saved at once and restored at launch.
- `Start`: save the current position, Arabic mode and Back history to the book's `.sav`, and retry pending global settings. The result is `Saved`, `Position save failed`, `Settings save failed` or `Both saves failed`.
- `Select`: Settings. Holding `Down` does nothing.

### Settings

- Rows while reading: Back to Files, Go to, Line spacing, Paragraph gap, L/R page turns, About. From Home: Line spacing, Paragraph gap, L/R page turns, About.
- `Up` / `Down`: move. `Left` / `Right` change a value; `A` steps it forward and wraps. Grey at the right: the current page (`Page ...`, then `Page about 142` until counted) and lines per page.
- Go to: `Left` / `Right` 1% (hold to repeat), `L` / `R` 10%; `A` jumps now. The jump starts at the beginning of that paragraph.
- `Back to Files`: close the book and return Home. When the page shown is not the last saved one (Start), the row asks `Continue without saving?` first: `A` leaves without saving, `B` or `Up`/`Down` keeps the book open. `About`: seven pages, `Left` / `Right`, `B` back.
- `B`, `Select` or `Start`: apply and return. Changed values are saved once; spacing/gap apply to every book.

History is reused only when its renderer marker and effective spacing/gap/Arabic match. Otherwise the exact saved byte anchor survives while history and page information rebuild. Back rebuilds incrementally from a nearby line start (about 8 KiB back, not the book start), prefetching older blocks. Early Back shows `Loading back...`; partial rebuilds restart safely at their anchor, not an incomplete scan.

### Arabic display

Shaping starts OFF for each document unless its own checked saved flag is ON. Opening in OFF mode checks only the page being resumed, or the first page. Finding Arabic enables shaping, relayouts that same anchor, and automatically saves ON in that book's SAV. Other files are unaffected. A failed save leaves ON active in RAM; keep the book open and retry Reader `Start`.

Later pages are not checked in an OFF session. To activate Arabic found later, save on a page containing it and reopen. Enabled Arabic joins right-to-left using native Ghoulam; Latin/digits stay left-to-right. Harakat are hidden, never deleted. Mixed direction and Persian/Urdu coverage are limited; no typing mode or manual OFF toggle is added.

### Files and automatic saves

Every book has its own companion: `Title.txt` uses `Title.txt.sav`; `Title.epub` uses `Title.epub.sav`. All newly generated per-book state and the EPUB normalized-text cache are in that SAV, not in the book. Book bytes stay unchanged. The book may be marked read-only, but its directory and companion must be writable. Leave sufficient free card space and keep backups.

An EPUB can show `Preparing cache...` on first opening, cache migration or rebuilding. This automatically saves its cache and current state in its SAV. Opening-page Arabic activation also saves automatically, combined with cache preparation when needed. Ordinary page turns do not save; later position/history changes require a successful Reader `Start` before leaving. Global-only saves never write a book bookmark, SAV or cache. An interrupted initial cache may leave a complete state without a usable cache; a later open rebuilds from the originals.

Global settings and the last-opened filename use `SETTINGS0.DAT` / `SETTINGS1.DAT` in `/gbareader`. A successful book open saves a changed filename even without visiting Settings or pressing Start; reopening the same book is a no-op when globals are clean. Recording a filename preserves preferences, and applying preferences preserves the filename. This remembers selection only, not a new bookmark.

### Moving, renaming and resetting

Copy or move the book AND its SAV together. When renaming `Title.epub` to `Novel.epub`, rename `Title.epub.sav` to `Novel.epub.sav` too. Do not reuse a same-name SAV after replacing or editing its book. Delete or move aside that SAV deliberately, after keeping a backup. Discovery accepts filenames up to 255 UTF-8 bytes, but companion saving requires a book filename of at most 251 bytes to leave room for `.sav`.

Deleting a SAV loses its latest position, Arabic flag, history and EPUB cache, not global preferences; the cache can be rebuilt. To carry preferences and Home selection to another card, copy both SETTINGS files too. Back up then move both aside to reset globals to defaults and no remembered book. Old embedded metadata, if present, was never removed: deleting the SAV can therefore restore an older embedded bookmark or Arabic flag rather than a completely fresh start. Use a clean original book if a complete reset is wanted.

### Migration and failed saves

Without a SAV, valid legacy TXT footers and EPUB state are read; TXT footers remain hidden from displayed text. A successful save imports state into the companion without removing old bytes. A usable embedded EPUB cache is read and migrated automatically; otherwise the cache is rebuilt from the original chapters. A valid matching SAV takes precedence. An unreadable, mismatched or unknown SAV (even empty) suppresses embedded state instead of reviving a possibly stale bookmark, and is not overwritten automatically.

After a position-save failure, keep the book open and retry Reader `Start`. A settings-save failure leaves edited globals and the remembered filename pending in RAM; retry on the next Settings close, successful book open or Reader `Start`. Pending changes are not durable. One save can succeed while the other fails. A transient source read error can be retried read-only after checking size and identity. A complete previous state bank can survive a torn update. A newly created empty SAV is retryable only while that book stays open; after reopening, an empty SAV has no ownership proof. Back up and move aside unknown or mismatched SAVs on a computer, including empty interrupted creations. A damaged record with a valid matching identity header remains retryable. Persistent media faults, arbitrary power loss and filesystem damage are not guaranteed recoverable. Never remove the card or switch off during storage operations.

Global loading selects the newest valid record, falling back to the other copy. `Settings recovered` warns of a damaged/unreadable partner; `Settings load failed` uses defaults when neither is usable. Unknown versions, unrelated collisions and empty files left by a previous process are preserved. Back them up and move them aside on a computer before retrying; do not remove the card during writes. Two copies cannot guarantee recovery from arbitrary media or filesystem damage.

### Compatibility and identity

EPUB support is text-only: no DRM, images, CSS presentation, scripts, embedded fonts, audio or video. Supported text uses UTF-8. The source identity uses file sizes, three bounded physical samples and, for EPUB, original central-directory metadata. It avoids a full-book hash on each open, but cannot detect every same-size edit outside those samples or malicious checksum replacement. Treat books as unchanged while their companions are in use; after any external edit, move aside the old SAV.

Requires compatible Supercard SD hardware. Host filesystem tests and emulator UI checks do not prove physical card operation. Keep backups of books, companions and both SETTINGS files.

## Hardware and files

The Supercard SD driver, FatFS configuration, fonts, normal page-turn inputs, Arabic geometry and existing OAM capacity assertions are retained. The shoulder-page-turn mode is global, never per-book, and the last choice is saved. Reading state uses the existing checked 800-byte serializer within the SAV banks, including history-layout metadata checked against the effective global-plus-book layout before reuse. Read-only legacy handling remains in the reader; the old embedded writer is retained only as a host fixture generator, not a device save route.

## Building and testing

Requirements: devkitPro/devkitARM, GNU Make, Python 3 and Butano. CI pins Butano `77dcbcb3d8783596a9f333c64eedbccec77b05dc`.

```sh
make clean LIBBUTANO=/absolute/path/to/butano/butano
make -j2 LIBBUTANO=/absolute/path/to/butano/butano
bash tests/run_host_tests.sh
EXTRA_CXXFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' bash tests/run_host_tests.sh
python3 tests/fatfs/run.py
GBAREADER_FATFS_FIXTURE=legacy-v047.epub python3 tests/fatfs/run.py
```

The runnable output is `gbareader.gba`. `make DEMO=1` builds `demo_gbareader.gba` (in `build_demo/`): it opens with a start screen (books found in `/gbareader`, Flashcart compatible and SD card checks), each book opens behind a "You are reading the demo version" screen, and saving the reading position and Go to show a notice pointing to the full version at halimj.itch.io instead. Import works, so the demo also checks that the card can be written. CI builds both. Real FAT image tests require GCC/G++, `dosfstools` and `mtools`; see [test documentation](tests/fatfs/README.md). The manual is written by hand in `docs/gbareader-full-controls.md` (screenshots in `docs/images/`); rebuild its PDF with `python3 docs/build_controls_pdf.py` (needs ReportLab, pypdf and Pillow).

The inherited `make test` frontend smoke wrapper is not sufficient boot evidence. It can report a pass after a display/frontend startup failure; verify actual application pixels independently. Physical Supercard testing remains separate.

## Architecture

- `reader_core`: UTF-8, wrapping, pagination and bounded history.
- `reader_file` / `reader_sidecar.inc`: read-only book handles, library discovery, bounded SAV banks, source association, readback and failure-only companion cleanup. A second FIL replaces the old 800-byte footer rollback buffer; the 8 KiB read window is borrowed exclusively for state/readback work.
- `reader_sidecar_cache.inc`: bounded normalized export and checksum-table construction; no cache rewrite for ordinary state saves.
- `epub_document`: ZIP/container/OPF parsing, chapter normalization, legacy/companion cache validation and checked lazy block fallback. The archive and external cache remain distinct byte sources.
- `reader_txt_save`: existing versioned state codec, reused without dropping byte/history/Arabic semantics.
- `reader_global_settings`: bounded explicit versioned global records, full remembered filename, checked alternating save/readback and dirty retry.
- `reader_screen` / `reader_menu`: gbamp3-style bitmap screens (header, rows, light-blue bar, 5x7 labels), list/repeat/marquee rules, Settings rows and About pages.
- `reader_browse`: import browser listing (folders first, windowed names) and the checked copy into `/gbareader`.
- `main`: scenes, global startup/open/apply saving, independent manual bookmark save and the saved L/R mode.

Body text and every menu use the double-buffered 8-bit bitmap; only the overlays over book text (`save...`, `L+R: On`) are sprites.

## Provenance and licensing

Made by Halim Jarrar · © 2026 · halimj.itch.io · gba@halim-jarrar.de

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). [Font source](https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1). Actual GSUB glyphs were converted to native 11px monochrome ROM rows; this modified representation is not author endorsement. See [Arabic policy and provenance](docs/arabic.md).

The project derives from [`gba-vocab-trainer-CC` v0.2.5](https://github.com/Xinon232/gba-vocab-trainer-CC/releases/tag/v0.2.5). Its Supercard/FatFS integration is retained; UI headers and labels use the 5x7 font from [gbamp3](https://github.com/Xinon232/gbamp3) v0.9.8 (generated by `tools/make_ui_small_font.py`). SuperFW font-rendering sources and font packs retain upstream notices under `references/superfw/`. The inherited UNSCII full-derived pack includes GNU Unifont and public-domain Fixedsys Excelsior glyphs; it is not wholly public domain.

The tinfl-only miniz files are vendored from `77d0dce8627735138c51770d1799a1ef48f2117d`, without allocation, compression, zlib, stdio, time or miniz archive APIs. Provenance and MIT license: `third_party/miniz/`.

The inherited project and SuperFW components are GPL v3 or later; see [LICENSE](LICENSE). Butano is zlib-licensed; FatFs retains ChaN's permissive notice. Credits also appear in the full-controls PDF and on the app's About pages (Settings > About).
