# gbareader V3.1

## Full controls

Read TXT and EPUB books, save your reading position, and resume later. Place UTF-8 `.txt` and supported `.epub` files directly in `/gbareader` on the SD-card root, then restart the app, or import them with `Start`. It lists every supported file (no limit), not subfolders, with eight books visible at once. A missing or empty folder shows `No books found`; `Start` imports books from anywhere on the card. This reader has no text editing, typing or user-facing export controls.

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

## Credits and font licenses

Made by Halim Jarrar
(C) 2026
halimj.itch.io
gba@halim-jarrar.de

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, CC BY 4.0. Converted actual contextual GSUB forms to native 11px monochrome ROM glyphs; this modified representation is not author endorsement. Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1 — License: https://creativecommons.org/licenses/by/4.0/.

SuperFW fonts, renderer and SD foundation: David Guillen Fandos, © 2024–2025, GPL v3 or later. https://superfw.davidgf.net/. The original SuperFW and supplemental font packs are unchanged.

UNSCII by Viznut: the included unscii-16-full-derived font pack is GPL, not wholly public domain. It incorporates GNU Unifont and public-domain Fixedsys Excelsior glyphs. https://viznut.fi/unscii/.

GNU Unifont / Hangul: Roman Czyborra, Paul Hardy and Unifont contributors. GPL v2 or later with the GNU font embedding exception. https://www.unifoundry.com/unifont/. The inherited exact font snapshot version is not recorded; no claim of eligibility for a newer alternative OFL license is made.

UI font: the 5x7 font from gbamp3 v0.9.8 by Halim Jarrar, part of this project family (GPL v3 or later). https://github.com/Xinon232/gbamp3.

Butano engine: Gustavo Valiente, zlib license. Built with devkitARM / devkitPro. FatFs © 2022 ChaN, permissive redistribution terms in src/ff.c. Framework dependency notices remain in the Butano source distribution.

miniz: MIT license; © 2010–2014 Rich Geldreich and Tenacious Software LLC; © 2013–2014 RAD Game Tools and Valve Software. Complete notice in third_party/miniz/LICENSE.

Based on gba-vocab-trainer-CC v0.2.5. Project license: GPL v3 or later. Full source and notices: https://github.com/Xinon232/gbareader. In-ROM Credits keeps the personal block on page one and third-party credits on the following six pages. Left/Right changes pages; B or Start closes.
