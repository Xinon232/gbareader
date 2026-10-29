# gbareader 10.0

## Full controls

Read TXT and EPUB books and comics, save your reading position, and resume later. Place UTF-8 `.txt`, supported `.epub` and `.gbcomic` files directly in `/gbareader` on the SD-card root, then restart the app, or import them with `Start`. It lists every supported file (no limit), not subfolders, with eight books visible at once. A missing or empty folder shows `No books found`; `Start` imports books from anywhere on the card. This reader has no text editing, typing or user-facing export controls.

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

## About / licence

gbareader 10.0

© 2026 Halim Jarrar · halimj.itch.io · gba@halim-jarrar.de

License: GPL-3.0-or-later. gbareader is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

No warranty. gbareader is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

Source code: github.com/Xinon232/gbareader

### Credits and font licenses

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, CC BY 4.0. Converted actual contextual GSUB forms to native 11px monochrome ROM glyphs; this modified representation is not author endorsement. Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1 — License: https://creativecommons.org/licenses/by/4.0/.

SuperFW fonts, renderer and SD foundation: David Guillen Fandos, © 2024–2025, GPL v3 or later. https://superfw.davidgf.net/. The original SuperFW and supplemental font packs are unchanged.

UNSCII by Viznut: the included unscii-16-full-derived font pack is GPL, not wholly public domain. It incorporates GNU Unifont and public-domain Fixedsys Excelsior glyphs. https://viznut.fi/unscii/.

GNU Unifont / Hangul: Roman Czyborra, Paul Hardy and Unifont contributors. GPL v2 or later with the GNU font embedding exception. https://www.unifoundry.com/unifont/.

UI font: the 5x7 font from gbamp3 v0.9.8 by Halim Jarrar, part of this project family (GPL v3 or later). https://github.com/Xinon232/gbamp3.

SD card drivers (gbaSD, https://github.com/Xinon232/gbaSD): SuperCard SD driver from SuperFW by David Guillen Fandos, GPL v3 or later; EverDrive GBA driver ported from edio-gba by krikzz, MIT license; EZ-Flash Omega driver by Halim Jarrar, GPL v3 or later, written after the EZ-Flash Omega kernels (Apache 2.0).

Butano engine: Gustavo Valiente, zlib license. Built with devkitARM / devkitPro. FatFs © 2022 ChaN, permissive redistribution terms in gbasd/fatfs/ff.c. Framework dependency notices remain in the Butano source distribution.

miniz: MIT license; © 2010–2014 Rich Geldreich and Tenacious Software LLC; © 2013–2014 RAD Game Tools and Valve Software. Complete notice in third_party/miniz/LICENSE.

Full source and notices: https://github.com/Xinon232/gbareader. The app lists these credits on its About pages (Settings > About); Left/Right changes pages, B closes.
