# gbareader V3.0

## Full controls

Read TXT and EPUB books, save your reading position, and resume later. Place UTF-8 `.txt` and supported `.epub` files directly in `/gbareader` on the SD-card root, then restart the app. It lists every supported file (no limit), not subfolders, with five books visible at once. A missing or empty folder shows instructions; Controls and Credits remain available. This reader has no text editing, typing or user-facing export controls.

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

## Credits and font licenses

Made by Halim Jarrar
(C) 2026
halim-jarrar.de
monday@halim-jarrar.de

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, CC BY 4.0. Converted actual contextual GSUB forms to native 11px monochrome ROM glyphs; this modified representation is not author endorsement. Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1 — License: https://creativecommons.org/licenses/by/4.0/.

SuperFW fonts, renderer and SD foundation: David Guillen Fandos, © 2024–2025, GPL v3 or later. https://superfw.davidgf.net/. The original SuperFW and supplemental font packs are unchanged.

UNSCII by Viznut: the included unscii-16-full-derived font pack is GPL, not wholly public domain. It incorporates GNU Unifont and public-domain Fixedsys Excelsior glyphs. https://viznut.fi/unscii/.

GNU Unifont / Hangul: Roman Czyborra, Paul Hardy and Unifont contributors. GPL v2 or later with the GNU font embedding exception. https://www.unifoundry.com/unifont/. The inherited exact font snapshot version is not recorded; no claim of eligibility for a newer alternative OFL license is made.

UI font: the 5x7 font from gbamp3 v0.9.8 by Halim Jarrar, part of this project family (GPL v3 or later). https://github.com/Xinon232/gbamp3.

Butano engine and cursor font: Gustavo Valiente, zlib license. Built with devkitARM / devkitPro. FatFs © 2022 ChaN, permissive redistribution terms in src/ff.c. Framework dependency notices remain in the Butano source distribution.

miniz: MIT license; © 2010–2014 Rich Geldreich and Tenacious Software LLC; © 2013–2014 RAD Game Tools and Valve Software. Complete notice in third_party/miniz/LICENSE.

Based on gba-vocab-trainer-CC v0.2.5. Project license: GPL v3 or later. Full source and notices: https://github.com/Xinon232/gbareader. In-ROM Credits keeps the personal block on page one and third-party credits on the following six pages. Left/Right changes pages; B or Start closes.
