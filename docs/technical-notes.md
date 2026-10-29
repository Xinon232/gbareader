# gbareader 10.0: technical notes

The player-facing overview is the [README](../README.md). This page keeps the detailed behaviour, limits and version history.

Read TXT and EPUB books and comics on your Game Boy Advance. Save your place and return to it later. Put books directly in `/gbareader` on a compatible Supercard SD card; they appear on Home.

## 10.0: About and licence, published source

- Released as 10.0 (developed as V3.3); the app, its About pages and the PDFs say 10.0. The `.sav`, `SETTINGS0/1.DAT` and `.gbcomic` formats are unchanged.
- About (Settings > About), page 1: `gbareader 10.0`, `(c) 2026 Halim Jarrar`, `GPL-3.0-or-later`, `No warranty.`, the source at github.com/Xinon232/gbareader, halimj.itch.io and gba@halim-jarrar.de; the License page and the credits pages follow (SD card drivers: SuperFW by David Guillen Fandos, edio-gba by krikzz, MIT, the Omega driver after the Omega kernel, Apache 2.0).
- Both PDFs end with an About / licence block: version, copyright, licence, no warranty, source code and credits.
- The complete source of the release is published at [github.com/Xinon232/gbareader](https://github.com/Xinon232/gbareader).

## V3.3 (released as 10.0): comics (.gbcomic), turned screens, settings per book

- **Comics as `.gbcomic`.** A comic (PDF, EPUB, CBZ or a folder of pictures) is converted on a computer with the gbcomic conversion software ([halimj.itch.io](https://halimj.itch.io/)). The converter scales every page once for each zoom: cropped, scaled straight from the original with a good filter, lightly sharpened and given its own 248 colours, for the upright and the sideways angles. The GBA only reads and unpacks the part it shows: the next screen of a page usually appears in the next frame, a new page in about 0.1 s. See [the file format](gbcomic-format.md).
- **Zooms per angle** (Settings > This book > View): Fit page, Fit width and, by default, Zoom 1.6x, 1.8x and 2x upright, 2.2x, 2.4x and 2.6x sideways (90°). The converter offers any of 1.1x to 3.0x for each angle.
- **Screens**: a fixed grid, the same on every page: as many as cover the page, spread evenly, or your own number of columns and rows (View screen; one choice for the comic: every zoom and both angles; `Auto (n)` shows the number each zoom needs). Fewer than needed: the parts not covered are shared out evenly before, between and after the screens (one screen sits in the middle); A/B pass over them. Reading order Left to right, Right to left (manga) or Columns; Skip empty.
- **Comic controls**: the D-pad moves around the picture; `A` / `B` go to the next / previous screen. After a D-pad move, `A` goes on from where you are: the next screen of the same row (at your height), or, at the end of the row, the first screen of the next row. `B` the same way back. The grid never changes.
- **Animations** (Settings, for all comics; the row is there while a comic is open): D-pad scrolling, A/B within a row and A/B to the next row can each be Static, Slide or Smooth (all Smooth by default). Only what comes into view is drawn, at 60 frames a second.
- **Page map** (On/Off, Settings, for all comics; the row is there while a comic is open): a small map shows where on the page the screen is, once the screen keeps still.
- **Angle** for every book, chosen from a list (Settings > This book > Angle): Normal, 90° right, 90° left or Upside down; the D-pad turns with the screen.
- Every book keeps its own Angle, View, screens and sliding settings in its `.sav`.
- `.gbcomic` files from the V3.3 test builds (version 1) must be converted again.
- CBZ files are not read on the GBA any more (convert them); EPUBs are always read as text.
- Planned: PDF.

## V3.2: Show file extensions

- New Settings row `Show file extensions` (default On). Off hides `.txt` / `.epub` in the Home book list (`Alice.txt` shows as `Alice`); only the shown names change, never the files. The choice is saved with the other global settings and restored at launch.

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

V3.2 publication is separate from updating `main`; unrelated historical releases remain unchanged. Technical details: [SAV format, limits and recovery](sidecar-format.md).

## Features and bounds

- Comics as `.gbcomic` files made by the gbcomic converter: up to 65535 pages; up to 48 zooms (every zoom the converter offers: 44); 248 colours per picture. The GBA reads them read-only, like every book.
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

Read TXT and EPUB books and comics, save your reading position, and resume later. Place UTF-8 `.txt`, supported `.epub` and `.gbcomic` files directly in `/gbareader` on the SD-card root, then restart the app, or import them with `Start`. It lists every supported file (no limit), not subfolders, with eight books visible at once. A missing or empty folder shows `No books found`; `Start` imports books from anywhere on the card. This reader has no text editing, typing or user-facing export controls.

See [the full controls](gbareader-full-controls.md).

### Home (file list)

- `Up` / `Down`: move (a fresh press on the first/last book wraps; holding repeats and stops at the ends). `Left` / `Right`: one page of eight books.
- `A`: open the selected book. Each successful open remembers its full filename; on next launch Home selects it again, without opening it. Missing or renamed books fall back to the first entry.
- `Select`: Settings. `Start`: import browser.
- `L+R` (pressed together): sleep. The screen goes off and the GBA uses almost no power until `Start` is held for 1 second, which wakes it, back on the same list (a shorter press, or `Start` let go for an instant during the second, leaves it asleep with the screen off). That `Start` press does nothing else, and every button is ignored until all have been let go for a moment. Only on Home: `L+R` held while the app starts (SuperCard driver) does not sleep; press them again once the list is shown.

### Import

- Lists the folders (with a folder icon, first) and TXT/EPUB/.gbcomic books of the current folder, starting at the SD root. Hidden/system entries and `/gbareader` itself are not shown.
- `A` on a folder opens it; `B` goes up one folder (from the root: back Home).
- `A` on a book asks `Import into /gbareader?` with `No` selected. `Yes` copies the book (progress in percent) and shows Home with the new book selected and `Imported`. `Already in /gbareader` means a file with that name exists there; it is left unchanged. A failed copy leaves no partial file.

### Reader

- `Right` or `A`: next page. `Left` or `B`: previous page.
- Hold `Up` alone (about 0.8 s): toggle shoulder page turns (`L` back, `R` forward). `L+R: On` / `L+R: Off` appears for about a second; the choice is saved at once and restored at launch.
- `Start` (when let go): save the current position, Arabic mode and Back history to the book's `.sav`, and retry pending global settings. The result is `Saved`, `Position save failed`, `Settings save failed` or `Both saves failed`. The D-pad does nothing while Start is held.
- `Select`: Settings. Holding `Down` does nothing.
- Every press counts: the keys are read 60 times a second, also while a page is still being laid out, and presses come in turn after it, in order (quick `A` then `B` presses end where the same presses made slowly end). `B` pressed while older pages load (`Loading back...`) goes back once for each press when they are there.
- With an Angle other than Normal the D-pad turns with the screen: `Right` is whatever points right on the turned page.

### Comics

Comics are `.gbcomic` files, converted on a computer first with the gbcomic conversion software ([halimj.itch.io](https://halimj.itch.io/)); CBZ files are not listed.

- `A`: next screen, then the next page. `B`: the screen before, then the page before. Screens with nothing but paper are passed over (`Skip empty`).
- `A` / `B` pressed during a slide: the next screen of the grid after the one the slide goes to (as if it had finished), sliding there from where the screen is (Smooth: at speed, without easing in again). Every press counts, also while a slide is loading.
- `L` / `R` held (when `L/R page turns` is on; the row reads `L/R zoom (held)` while a comic is open): `R` shows the comic at the next bigger zoom of its angle, `L` at the next smaller one, centred where the screen was (during a slide: on the screen it goes to), as long as the button is held (both held cancel out: its own zoom); let go, it goes back to its own zoom: exactly where it was if nothing moved meanwhile, else around the same place. D-pad, `A` and `B` work as usual meanwhile. With no bigger zoom `R` does nothing, with no smaller one `L` does nothing. Opening Settings ends it. In comics `L` / `R` no longer do what `A` / `B` do.
- D-pad: move around the picture (D-pad scrolling Static: 16 pixels a press, hold to repeat; Slide: while held; Smooth: faster the longer it is held, gliding to a stop in even steps: 6, 6, 4, 4, 2, 2 pixels from full speed). The screens stay a fixed grid. After a move, `A` goes to the next screen of the same row that starts beyond the view, keeping its height; at the row's end (the view at the picture's right edge) to the first screen of the next row (at that row's height, or lower if you moved down past it). `B` the same way back (left edge: the last screen of the row above). Right to left mirrors this; Columns goes down the columns. Holding `Up` does not switch L/R page turns in a comic. A D-pad held while `A` / `B` is pressed does nothing until it is let go (the slide plays, the view stays). A D-pad pressed during a slide stops it where the screen is and moves on from there.
- `Start` (when let go): save the page. `Select`: Settings (during a slide: closed again, the screen it was going to).
- With `Page map` on, a small map at the top right shows where on the page the screen is for about a second after it changed.

### Settings

- Rows while reading: Back to Files, Go to, Line spacing, Paragraph gap, L/R page turns, Show file extensions, Page map and Animations (only while a comic is open), About, then under `This book`: View (comics only) and Angle. A comic has no Line spacing or Paragraph gap rows, and Go to is a page number (`L` / `R` 10 pages). From Home: Line spacing, Paragraph gap, L/R page turns, Show file extensions, About.
- `Angle`: `A` opens the Angle screen: Normal, 90° right, 90° left and Upside down, the angle chosen ticked. `Up` / `Down` move, `A` chooses: the screen and the D-pad turn at once and the cursor stays on the angle; `B` returns to Settings. A comic converted for one kind of angle only (upright or sideways) shows the others grey, and the cursor skips them. On the Settings row `Left` / `Right` step through the angles directly.
- `View` (comics): `Left` / `Right` change the zoom (of the angle chosen); `A` opens the View screen. There: `A` chooses a zoom (those in the file for this angle) or a reading order (Left to right, Right to left, Columns); `Columns` and `Rows` set the number of screens (`Auto (n)`, or 1 to 9; `Left` / `Right` / `A`); the choice stays for every zoom and both angles (`Auto (n)` shows how many each zoom needs); `Skip empty` passes over screens with nothing but paper. The picture shows the current page cut into numbered screens, for the row the cursor is on. `B` returns to Settings.
- `Page map`: On / Off, for all comics (global, never used by TXT or EPUB); the row is shown only while a comic is open. `Animations` (all comics, global like Page map; shown only while a comic is open; `A`): D-pad scrolling, A/B same row and A/B next row, each Static, Slide or Smooth (`Left` / `Right` / `A`); `B` returns.
- `Show file extensions` (default On): Off hides `.txt` / `.epub` in the Home book list. Only the shown names change; the files are untouched. Saved with the other global settings.
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

Runs on the SuperCard SD, EZ-Flash Omega (also the Definitive Edition; start the ROM from the SD card, not from NOR) or EverDrive GBA. Host filesystem tests and emulator UI checks do not prove physical card operation. Keep backups of books, companions and both SETTINGS files.

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

The runnable output is `gbareader.gba`. Real FAT image tests require GCC/G++, `dosfstools` and `mtools`; see [test documentation](../tests/fatfs/README.md).

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
- `gbcomic`: `.gbcomic` books: zooms, views, ink grids, unpacking (BIOS LZ77 format, IWRAM) and drawing any part of a picture from its strips. The gbcomic converter makes the files.
- `comic_layout`: where the screens go (counts, even spreading, reading order, ink cells).
- `comic_region` / `comic_reader`: the decoded part of the picture around the screen (loaded ahead while there is time), and the comic on screen: A / B on the grid after D-pad moves, sliding (in even steps across the screen: an odd one is a pixel-by-pixel copy, about three times slower; a press during a slide slides on from where the screen is), and drawing only what comes into view (turned copies in IWRAM, `reader_turn.iwram.cpp`).
- `reader_keys` / `reader_key_latch`: the keys as the reader sees them (the D-pad turned with the screen), read in the VBlank interrupt (`reader_keys.iwram.cpp`, ahead of Butano's own handler) and kept in order (up to 511 changes), so the main loop takes every press even when a frame of it takes longer than a frame.
- `main`: scenes, global startup/open/apply saving, independent manual bookmark save and the saved L/R mode.

Body text and every menu use the double-buffered 8-bit bitmap; only the overlays over book text (`save...`, `L+R: On`) are sprites.

## Provenance and licensing

Made by Halim Jarrar · © 2026 · halimj.itch.io · gba@halim-jarrar.de · GPL-3.0-or-later, no warranty · source: [github.com/Xinon232/gbareader](https://github.com/Xinon232/gbareader)

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). [Font source](https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1). Actual GSUB glyphs were converted to native 11px monochrome ROM rows; this modified representation is not author endorsement. See [Arabic policy and provenance](arabic.md).

The project derives from [`gba-vocab-trainer-CC` v0.2.5](https://github.com/Xinon232/gba-vocab-trainer-CC/releases/tag/v0.2.5). Its Supercard/FatFS integration is retained; UI headers and labels use the 5x7 font from [gbamp3](https://github.com/Xinon232/gbamp3) v0.9.8 (generated by `tools/make_ui_small_font.py`). SuperFW font-rendering sources and font packs retain upstream notices under `references/superfw/`. The inherited UNSCII full-derived pack includes GNU Unifont and public-domain Fixedsys Excelsior glyphs; it is not wholly public domain.

The tinfl-only miniz files are vendored from `77d0dce8627735138c51770d1799a1ef48f2117d`, without allocation, compression, zlib, stdio, time or miniz archive APIs. Provenance and MIT license: `third_party/miniz/`.

The inherited project and SuperFW components are GPL v3 or later; see [LICENSE](../LICENSE). Butano is zlib-licensed; FatFs retains ChaN's permissive notice. Credits also appear in the [full controls](gbareader-full-controls.md) and on the app's About pages (Settings > About).
