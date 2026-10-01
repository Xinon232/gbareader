
# gbareader V3.2

Based on V3.1.1 (`v3.1.1`). One new setting; reading, controls and `.sav` companions are unchanged.

## New

- `Show file extensions` row in Settings (Home and while reading, after `L/R page turns`). Default On. Off hides the extension in the Home book list: `Alice.txt` shows as `Alice`. Only the shown names change; the files on the card, opening books and the remembered last book are untouched.
- `A` flips it, `Right` turns it On, `Left` Off. Saved with the other global settings in `SETTINGS0/1.DAT` when Settings closes, and restored at launch.

## Compatibility

- Settings files from earlier versions load as On. The flag uses a spare byte of the existing format (0 = shown), so the files stay the same size.
- An older gbareader reads a settings file saved with the setting Off as damaged (`Settings recovered` / `Settings load failed`, defaults). Saved with it On, older versions read it as before.

## Tests

- Host tests cover the new row, its drawing and keys, the saved byte and the name shortening; all pass.
- The ROM ran on mGBA with a modeled Supercard SD card: the toggle, the Home list without extensions, and the setting after a restart. Not physical hardware proof.

Physical hardware has not been tested with this version.
# gbareader V3.1.1

Same app as V3.1 (`v3.1.0`); only the build changed. The app itself still shows V3.1.

- Reproducible ROM: the build no longer stores its folder path in the ROM, so the same source gives a byte-identical `gbareader.gba` in any folder, repository or computer (devkitARM image `devkitpro/devkitarm:20260610`, butano `77dcbcb`). Compare your own build with `SHA256SUMS`.
- The V3.1 ROMs built before this differ only in that stored path; reading, storage and controls are identical.

# gbareader V3.1

Based on V3.0 (`v3.0.0`). A visual redesign after gbamp3 v1.8, an import browser, and Settings on Select. Reading, storage (`.sav` companions, `SETTINGS0/1.DAT`) and the book page are unchanged.

## New

- gbamp3 look on every menu screen: small centred header, eight rows, light-blue selected row, long names scrolling on the selected row; no on-screen instructions.
- Home shows the book list at once (cursor on the last opened book); Up/Down wrap on a fresh press and repeat when held, Left/Right page.
- `Select` opens Settings (Home and reading). While reading, the first row is `Back to Files`, the way back to the file list. If the page shown is not the saved one, it asks `Continue without saving?` in that row: `A` leaves, `B` returns to `Back to Files`.
- `Start` on Home: import browser from the SD root (`Import to /gbareader`), folders first; `Import into /gbareader?` (No/Yes) copies a TXT/EPUB book into `/gbareader`, creating the folder if needed, never overwriting, removing a partial copy on failure.
- `L/R page turns` row in Settings; the last choice (Settings or hold Up while reading) is saved and restored at launch. `L/R on startup` is gone.
- About rewritten: About (V3.1, halimj.itch.io, gba@halim-jarrar.de), Controls (the four essential controls), License, Text and fonts, Arabic font, SD card and files, EPUB and engine.

## Removed

- Holding Down while reading (it opened Settings); Select now does that.
- The Controls (11 pages) and Credits (7 pages) screens, replaced by About.
- A missing `/gbareader` is no longer an error: Home shows `No books found` and Start imports.

## Tests

- Host tests updated for the new code (list rules, Settings, About text widths/notices, screen pixels, import paths); all pass, also with ASan/UBSan, and the FatFS image tests.
- New optional `tests/emu/screens.py`: the exact ROM on mGBA with a modeled Supercard SD card; screenshots of every screen and a real import onto the card image. Not physical hardware proof.

Physical hardware has not been tested with this version.

# gbareader V3.0 (pre-release)

Based on V1.9 (`v1.9.0`). Home shows only `gbareader`; V3.0 appears on the last Credits page, after the unchanged personal author page. Reading, storage (`.sav` companions) and controls are unchanged.

## Preserved V1.9: no limit on the number of books

- Home now lists every supported TXT/EPUB file in `/gbareader`. Earlier versions stopped at 64 files.
- Names are kept in RAM 64 at a time (the same memory as before) and reread from the folder when you move past them: about one folder read per 32 books scrolled. Books stay in folder order.
- If the folder cannot be read while scrolling, the row shows `?` instead of a wrong name.

Physical hardware has not been tested with this version.
