# gbareader V3.0

Read TXT and EPUB books on your Game Boy Advance. Save your place and return to it later. Put books directly in `/gbareader` on a compatible Supercard SD card; they appear on Home.

## V3.0: global settings and last-book selection

Line spacing, paragraph gap and `L/R on startup` are now shared across books and saved when Settings closes. Home starts with the last successfully opened book selected (never auto-opened), or the first book if that exact filename is missing. Two small checked files, `/gbareader/SETTINGS0.DAT` and `SETTINGS1.DAT`, hold these globals independently of each book's position/Arabic/cache. Hold Up remains a temporary session toggle. Home still shows only `gbareader`; V3.0 remains on the last Credits page.

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

V3.0 publication is separate from updating `main`; unrelated historical releases remain unchanged. Technical details: [SAV format, limits and recovery](docs/sidecar-format.md).

## Features and bounds

- Case-insensitive UTF-8 `.txt` and a bounded text-only subset of EPUB 2/3; any number of files directly in `/gbareader` (names are read from the folder 64 at a time), five visible at once. No subfolder browser, root fallback or demo book.
- Filenames remain intact and are clipped only on screen. Discovery accepts up to 255 UTF-8 bytes; saving needs room for `.sav`, so the full book filename must be at most 251 UTF-8 bytes. Longer names remain readable but cannot be saved until renamed on a computer.
- Streaming FatFS reads through an 8 KiB window; no whole-book RAM allocation.
- Stored/raw-DEFLATE ZIP entries, container/OPF manifest and declared spine order; URI percent-decoding and XML entities in references; preferred package selection for multiple rootfiles.
- Visible XHTML text including CDATA, block/table breaks and common XML/HTML entities. Images and image-only spine pages are skipped.
- Limits remain 256 readable spine documents, 255-byte internal paths, 64 KiB uncompressed metadata, 32 MiB uncompressed XHTML per spine document, 16 MiB compressed required entry and 128 MiB source archive. A 32 KiB inflater dictionary and 16 KiB text window bound chapter processing. Applicable errors are reported, never silently truncated.
- ZIP64, multi-disk archives, encrypted required entries and unsupported required compression are rejected. UTF-16 XML/XHTML, DRM, CSS presentation, scripts, embedded fonts, audio, video and SVG presentation are outside scope.
- Native 16px SuperFW body font, the gbamp3 5x7 UI font (with the blue Butano `>` cursor), supported Latin/Greek/Cyrillic/Japanese/CJK/Hangul and supplemental publishing symbols. Smart quotes and selected hyphens use the existing display-only ASCII substitutes.
- Word wrapping, CRLF/LF, UTF-8 BOM and safe malformed-input replacement. Runs of three or more spaces collapse on screen; repeated newlines collapse to one line break. None of this edits the source.
- Line spacing from 0 to 4 and paragraph gap None/Small/Half/Full, with text centered vertically; page number, percentage and Go to percentage in settings; byte anchors rather than saved page numbers; incremental Back-history rebuilding when layout markers or effective spacing/gap/Arabic differ.
- Native Ghoulam Arabic joining/lam-alef and limited per-line RTL. Harakat are hidden only on screen. Latin and digit runs remain left-to-right. This is not full Unicode bidi or full Persian/Urdu support.

## Controls

Read TXT and EPUB books, save your reading position, and resume later. Place UTF-8 `.txt` and supported `.epub` files directly in `/gbareader` on the SD-card root, then restart the app. It lists every supported file (no limit), not subfolders, with five books visible at once. A missing or empty folder shows instructions; Controls and Credits remain available. This reader has no text editing, typing or user-facing export controls.

See [the full controls PDF](docs/gbareader-full-controls.pdf) and [downloadable instructions Markdown](docs/gbareader-full-controls.md).

### Library

- `Up` / `Down`: select a `.txt` or `.epub` book.
- `A`: open the selected book. Each successful open remembers its full filename, including the extension. On next launch Home selects the last successfully opened book and scrolls it into view, without opening it. Missing, renamed or unremembered books fall back to the first entry; cursor movement and failed opens are not remembered.
- `Select`: show Controls. `Left` / `Right` changes its eleven help pages; `B` returns Home.
- `Start`: show Credits, beginning with the personal author page. `Left` / `Right` changes its seven pages; `B` or `Start` returns Home. Font/framework/license attribution is on subsequent pages.

### Reader

- `Right` or `A`: next page.
- `Left` or `B`: previous page.
- Hold `Down` alone: open reader settings.
- Hold `Up` alone: toggle shoulder-button page turns. `L` goes back and `R` forward when enabled. A UI-font `L+R: On` or `L+R: Off` notice appears for about one second. The saved `L/R on startup` preference initializes it once at launch (default Off). Hold Up is never saved and survives book switches; changing the startup preference affects only the next launch.
- `Start`: save the current byte position, Arabic mode and up to 64 previous page offsets to the book's `.sav`; also independently retry pending global settings. After `save...`, the result is `Saved`, `Position save failed`, `Settings save failed` or `Both saves failed`. No cross-file atomic save is promised.
- `Select`: close the book and return Home without saving later position changes.

Both holds require 48 consecutive held updates after the initial press (about 0.8 seconds), exactly matching gbawriter R-only Caps. Short presses do nothing. Each hold acts once; release before trying again. Any other key or leaving reading cancels that hold. Library/settings Up/Down stay immediate.

History is reused only when its renderer marker and effective spacing/gap/Arabic match. Otherwise the exact saved byte anchor survives while history and page information rebuild. Back rebuilds incrementally from a nearby line start (about 8 KiB back, not the book start), prefetching older blocks. Early Back shows `Loading back...`; partial rebuilds restart safely at their anchor, not an incomplete scan.

### Settings

- `Up` / `Down`: select line spacing (0 through 4 pixels), paragraph gap (None, Small, Half or Full line), `L/R on startup` (Off/On), or Go to. Grey Lines per page and page/percentage readouts are information only; the blue cursor skips them. While the page count runs in the background (also while Settings is open) it shows `Page ...`, then an estimate such as `Page about 142`, until the exact number is known. After Go to, pages already counted are kept.
- `Left` / `Right`: change the selected value. On Go to, `L` / `R` change it by 10%.
- Go to: press `A` to jump to the chosen percentage straight away (closing settings with `B` or `Start` also jumps if the value changed). The jump starts at the beginning of that paragraph.
- `B` or `Start`: apply and return. All three exit routes (including A on Go to) save changed global preferences once; unchanged or reverted values do not write. Spacing/gap apply to every book, defaulting to 1/Full when no config exists, never to the first book's old values. Layout changes preserve the byte anchor and rebuild incompatible history, not the EPUB text cache. Startup-only edits do not change layout or the current shoulder mode. SuperFW remains native 16px and Arabic Ghoulam native 11px.

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

The Supercard SD driver, FatFS configuration, fonts, normal page-turn inputs, Arabic geometry and existing OAM capacity assertions are retained. The live shoulder-page-turn mode is session-only; its startup default is global, never per-book. Reading state uses the existing checked 800-byte serializer within the SAV banks, including history-layout metadata checked against the effective global-plus-book layout before reuse. Read-only legacy handling remains in the reader; the old embedded writer is retained only as a host fixture generator, not a device save route.

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

The runnable output is `gbareader.gba`. Real FAT image tests require GCC/G++, `dosfstools` and `mtools`; see [test documentation](tests/fatfs/README.md). Regenerate the full PDF and Markdown with `python3 docs/build_controls_pdf.py` in an environment containing ReportLab and pypdf.

The inherited `make test` frontend smoke wrapper is not sufficient boot evidence. It can report a pass after a display/frontend startup failure; verify actual application pixels independently. Physical Supercard testing remains separate.

## Architecture

- `reader_core`: UTF-8, wrapping, pagination and bounded history.
- `reader_file` / `reader_sidecar.inc`: read-only book handles, library discovery, bounded SAV banks, source association, readback and failure-only companion cleanup. A second FIL replaces the old 800-byte footer rollback buffer; the 8 KiB read window is borrowed exclusively for state/readback work.
- `reader_sidecar_cache.inc`: bounded normalized export and checksum-table construction; no cache rewrite for ordinary state saves.
- `epub_document`: ZIP/container/OPF parsing, chapter normalization, legacy/companion cache validation and checked lazy block fallback. The archive and external cache remain distinct byte sources.
- `reader_txt_save`: existing versioned state codec, reused without dropping byte/history/Arabic semantics.
- `reader_global_settings`: bounded explicit versioned global records, full remembered filename, checked alternating save/readback and dirty retry.
- `main`: Butano UI, global startup/open/apply saving, independent manual bookmark save and session-only hold mode.

Body text uses the existing double-buffered bitmap renderer rather than one OAM sprite per glyph. The menu remains sprite-based.

## Provenance and licensing

Made by Halim Jarrar · © 2026 · halim-jarrar.de · monday@halim-jarrar.de

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). [Font source](https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1). Actual GSUB glyphs were converted to native 11px monochrome ROM rows; this modified representation is not author endorsement. See [Arabic policy and provenance](docs/arabic.md).

The project derives from [`gba-vocab-trainer-CC` v0.2.5](https://github.com/Xinon232/gba-vocab-trainer-CC/releases/tag/v0.2.5). Its Supercard/FatFS integration is retained; its Butano UI font now only draws the `>` cursor, and UI text uses the 5x7 font from [gbamp3](https://github.com/Xinon232/gbamp3) v0.9.8 (generated by `tools/make_ui_small_font.py`). SuperFW font-rendering sources and font packs retain upstream notices under `references/superfw/`. The inherited UNSCII full-derived pack includes GNU Unifont and public-domain Fixedsys Excelsior glyphs; it is not wholly public domain.

The tinfl-only miniz files are vendored from `77d0dce8627735138c51770d1799a1ef48f2117d`, without allocation, compression, zlib, stdio, time or miniz archive APIs. Provenance and MIT license: `third_party/miniz/`.

The inherited project and SuperFW components are GPL v3 or later; see [LICENSE](LICENSE). Butano is zlib-licensed; FatFs retains ChaN's permissive notice. Complete current credits also appear in the full-controls PDF and the app's seven Credits pages.
