# gbareader 10.0

Based on V3.2 (`v3.2.0`); developed as V3.3 and released as 10.0. Comics, turned screens and settings per book. Text reading, global settings and existing `.sav` companions keep working; the About page says 10.0.

## About and licence

- Version 10.0 everywhere it is shown: the About page, the PDFs, this file and the README. The `.sav`, `SETTINGS0/1.DAT` and `.gbcomic` formats keep their version numbers.
- About page 1: `gbareader 10.0`, `(c) 2026 Halim Jarrar`, `GPL-3.0-or-later`, `No warranty.`, `Source: github.com/Xinon232/gbareader`, halimj.itch.io, gba@halim-jarrar.de. The License page says the same at more length, and the SD card page credits the EverDrive driver (edio-gba by krikzz, MIT) and the Omega driver (Halim Jarrar, after the Omega kernel, Apache 2.0).
- The controls PDF and the instructions PDF end with an About / licence block (version, copyright, licence, no warranty, source code, credits).
- The complete source of this release is published at [github.com/Xinon232/gbareader](https://github.com/Xinon232/gbareader).

## New

- Comics as `.gbcomic` files (format version 2). The gbcomic conversion software ([halimj.itch.io](https://halimj.itch.io/)) converts a CBZ, a folder of pictures or a PDF on a computer: every page cropped, scaled once for each zoom straight from the original, lightly sharpened, 248 colours per picture, stored in strips packed in the GBA BIOS LZ77 format, with a grid of where the ink is. The GBA only reads and unpacks the part it shows.
- Zooms per angle: Fit page, Fit width and, by default, 1.6x, 1.8x and 2x upright, 2.2x, 2.4x and 2.6x sideways (`--normal`, `--sideways`: any of 1.1 to 3.0 for each angle, up to 44 in all).
- The reader places the screens as a fixed grid: as many as cover the page, spread evenly, or a number of columns and rows set on the View screen (`Auto (n)` or 1–9; one choice for the comic, kept for every zoom and both angles). Too few for the page: the parts not covered are shared out evenly before, between and after the screens. Reading order: Left to right, Right to left, Columns. Skip empty (from the ink grid).
- Comic controls: the D-pad moves around the picture; `A` / `B` go through the screens. After a D-pad move, `A` goes to the next screen of the same row beyond the view (keeping its height), or at the row's end to the first screen of the next row; `B` the same way back; mirrored for right to left, down the columns for Columns. The grid never changes. Holding Up does not switch L/R page turns in a comic. `Start` saves when it is let go.
- Hold zoom in comics: with `L/R page turns` on (the row reads `L/R zoom (held)` while a comic is open), holding `R` shows the comic at the next bigger zoom of its angle and holding `L` at the next smaller one, centred on the same place, until it is let go; then the comic's own zoom, exactly where it was if D-pad, `A` and `B` were not used meanwhile, else at the same place. D-pad, `A` and `B` keep working. `R` does nothing at the biggest zoom, `L` nothing at the smallest; both held cancel out (its own zoom); opening Settings ends it. `L` / `R` no longer turn screens in comics (books keep L/R page turns). The converter is unchanged.
- Animations (a general setting for all comics, listed under Page map while a comic is open): D-pad scrolling, A/B same row and A/B next row, each Static, Slide or Smooth (all Smooth by default). Only what comes into view is drawn (the screen shown is copied over by the move), and the parts ahead are loaded while there is time, so sliding runs at 60 frames a second.
- Page map On/Off (a general setting for all comics, listed while a comic is open): a small map shows where on the page the screen is, once it keeps still.
- Row changes in comics no longer stutter. A slide to another row passes over the area of both screens, which often does not fit in the picture memory, so part of it is read from the card on the way; frames that had to read a lot stalled (9 of 29 frames in a 1.6x row change). Now the way is prefetched as far as it fits, the picture memory is 16 KiB larger (Import borrows the book memory instead of its own buffer; no book is open then), and a slide that still has to read takes as long as reading needs at an even speed: 33-34 frames without a stall in the same row change. Slides that read nothing keep their speed and Smooth easing (20 frames). Waiting at a screen, the reader prepares the screen in the direction of the last A or B, so going back with B gets the same (before: always the next screen, so a row up with B read more and stalled up to 5 times). After a D-pad move to the picture's edge, the reader prepares exactly the screen `A` or `B` goes to from there (the D-pad direction counts as the way you are heading), and while sliding it reads the strips and rows just coming into view first: in the emulator, edge-then-A/B slides went from up to 8 stalls to none (two late stalls left in one of twelve measured slides).
- Fixed: large books stored in many pieces on the card (a 1000-page comic, a big EPUB) made everything slow: menus, slides, page turns. FatFs found each read's place by following the file's whole cluster chain. Every book (not only comics) now gets FatFs fast seek with room for 127 pieces, and a file in more pieces gets the cluster at 256 points noted once when it opens, so each read starts from the nearest one. Measured in the emulator with a 1200-page comic in 240 pieces: Settings opened after 131 frames, now 4 (as for a comic in one piece); a 40 MB EPUB that never finished opening now opens. Also kept after a read error reopens the book (fast seek was lost there before).
- Fixed: on a fragmented **exFAT** card a power cut while the `/gbareader` folder grew by a cluster could leave the folder unreadable (or new entries invisible on a PC). Two small exFAT-only FatFs changes (now in gbaSD's FatFs, marked `gbaSD:` in `gbasd/fatfs/ff.c`): a new piece of a chain is ended in the FAT before it is linked, and an exFAT folder ends at its recorded size. Checked by `gbasd/tests/fatfs/stretch_cut.py` (a cut at every write, then `fsck.exfat` and an independent reader).
- Fixed: on an **exFAT** card a power cut while the reader saved (a `.sav`, `SETTINGS0/1.DAT`, an Import) could leave a name half written (exFAT writes a name's directory entries a sector at a time); then nothing in `/gbareader` could be listed, opened or saved until the card was repaired on a PC. The reader now repairs the root and `/gbareader` when it starts, and again if a lookup there fails that way (`gbasd/fatfs/exfat_repair.c`: half-written names are removed, the last complete state is kept; files and their data are never touched; nothing is done on FAT). Also a FatFs fix: a new long name that needs two new clusters of its folder (512-byte clusters) now counts both in the folder's size. Checked by `gbasd/tests/fatfs/exfat_repair` (3671 cut points), `tests/exfat/entry_cut.py` (a cut at every write of the reader's own saves: 2107 cuts, after each the app starts, reads everything, saves again, `fsck.exfat` clean) and on the emulator by `tests/exfat/torn_emu.py`.
- FatFs now comes from gbaSD (`gbasd/fatfs`: R0.15 with the exFAT fixes above, `exfat_repair` and the disk glue, shared with the author's other GBA apps); the reader keeps only `include/ffconf.h`, and its exFAT card tests use gbaSD's test kit (`gbasd/tests/exfat_kit`). New with it, on **FAT16/FAT32**: when a folder grows by a cluster the new cluster is cleared before the folder is linked to it (before: linked first, so a power cut in between could leave old file data in `/gbareader` as garbage names); checked by `gbasd/tests/fatfs/growth_cut.py` with the reader's `ffconf.h` (`tests/exfat/run.py`).
- gbaSD 9cc52f8: a power cut while deleting or renaming a file on **FAT12/FAT16/FAT32** no longer leaves an 8.3 alias name (such as `ADIARY~1.TXT`): FatFs now deletes the short-name entry first, then the long-name entries (a cut leaves only unused long-name entries, which are skipped). exFAT is unchanged. Checked by `gbasd/tests/fatfs/remove_cut.py` with the reader's `ffconf.h` (`tests/exfat/run.py`). Nothing else changes on FAT: the screens and files of the emulator tests are the same as before.
- Fixed: books in many pieces on **exFAT** cards. The quick FAT scan that makes the seek map when a book opens was for FAT16 and FAT32 only, so on exFAT FatFs followed the chain a FAT sector at a time (772 reads instead of 28 in the card test); exFAT books in one piece (no FAT chain) keep FatFs fast seek, which needs no reads. And a short book (up to 512 clusters, such as a 2 MB comic on 4 KiB clusters) in more than 127 pieces got no seek map at all: each read followed its chain again. Now it gets noted points like a large book. In the emulator, a comic stored a cluster at a time on a 4 KiB-cluster card stalled in 98 frames of a slide test on exFAT and 84 on FAT16, instead of 41 in one piece; now 45 on exFAT. Tested on FAT and exFAT cards (modeled), with files in one piece and in pieces (`tests/exfat/run.py`).
- Fewer stalls while sliding in comics. Waiting at a screen, the reader loads the next one, but only as much as it reckoned fits in the rest of a frame; a piece of a picture bigger than that (common in detailed pages: about 1800 bytes against about 1500) was never loaded, however long you waited, so the next slide read it all on the way and stopped now and then (worst after a page turn: 68 frames with 12 stops instead of 20). Standing still, it now loads at least one piece a frame. And a slide to another row that reads on its way is now timed so that every part of the way is read by the time the screen gets there (what was loaded before lies near the start, so the reading piles up at the end), instead of spreading the reading evenly. Emulator, a 5-page comic read through with A: 924 frames with 29 stalled → 944 with 4, with a slower modeled card 141 → 69 stalled frames; back with B 43 → 4; on the EZ-Flash Omega model 28 → 12. Some slides that read a lot are a little longer instead.
- Faster opening of large books (all kinds, not only comics): the map of where the file lies on the card was made by following its whole chain of clusters in the FAT one FatFs call at a time. The FAT is now read 14 sectors at a time and scanned in IWRAM, skipping whole stretches of "next cluster" entries at once; the map is the same (checked on FAT16 and FAT32 cards in one piece, a few and many pieces). A 400 MB comic on a FAT32 card with 4 KiB clusters: the first screen after 37 frames instead of 158 (emulator; a 1.6 MB comic: 14). What is left is mostly reading the FAT itself.
- Fixed: after a slide one pixel column could be wrong (seen in lettering), and the page map's frame missed pieces at the page's edges. Single pixels written into screen memory now always use 16-bit stores (an 8-bit store lands in both pixels of a pair), and the screen's place stays inside the map's frame. The map no longer blinks once when a page opens.
- Quick presses in comics keep the screen moving. Slides now move in even steps across the screen (2 pixels or more; an odd step is a pixel-by-pixel copy, about three times slower, and was almost half the frames of a slide), so far less of a frame goes into moving the picture. `A` / `B` pressed during a slide go to the next screen of the grid after the one the slide goes to (the screens are the same as before), but now slide there from where the screen is, at speed (Smooth without easing in again): before, the screen jumped to the old slide's end, which was never loaded, and the next slide read the whole screen first. D-pad Smooth slows down in even steps too (6, 6, 4, 4, 2, 2 pixels from full speed; before 7, 6, 5, 4, 3, 2, 1). A D-pad press during a slide stops it where the screen is (before: it jumped to the slide's end). Emulator, a detailed comic at 3x read on with `A` every 12 frames: 108 of 292 frames did not move, the longest freeze 9 frames, now 42 of 262 and 3; every 6 frames 75 (10) → 23 (3); back with `B` every 6 frames 79 (11) → 36 (1); with a slow modeled card the longest freeze 14-15 → 5-6 frames. What is left is reading on the way while the screen still has to catch up.
- Every press counts. The keys were read once a frame of the app, so a press shorter than a slow frame (a page laid out, a comic read from the card) was lost. They are now read 60 times a second in the VBlank interrupt and handed over in order, one press at a time, held keys and `Start` on release as before. Emulator, a 4 MB TXT (a page turn takes 5-6 frames): 60 `A` presses 6 frames apart turned 43 pages, 3 frames apart 25; now 60 and 60, and 150 `A` then 150 `B` 3 frames apart end where the same presses made slowly end (before: 63 pages forward, 62 back). In a comic, 20 quick `A` presses (held 1-2 frames, 3-6 apart) moved 8-13 screens; now 20. `B` pressed while older pages load (`Loading back...`) now goes back once for each press (before once in all).
- Fixed: opening Settings during a slide in a sideways comic could leave pieces of Settings and old bands in the picture: the slide went on after Settings closed, copying from the picture memory that Settings had drawn over. Closing Settings now shows the screen the slide was going to.
- Fixed: holding `L` / `R` (zoom) during a slide left the view between two screens when let go; the held zoom now centres on the screen the slide goes to.
- More free RAM (EWRAM free 6,012 → 36,664 bytes): Butano's tables are cut to what the app uses (64 sprites, one background, no H-Blank effects), and the background page scans (Back history, page number) and saved states keep only where a page starts and ends instead of a whole laid-out page. The `.sav` format is unchanged.
- Angle for every book, chosen on its own screen like View (Settings > This book > Angle: Normal, 90° right, 90° left, Upside down; the angle chosen ticked, `A` turns the screen at once); the D-pad turns with the screen. L/R page turns are never switched on by turning. A comic shows the angles it was not converted for in grey, and the cursor skips them. `Left` / `Right` on the Settings row still step through the angles.
- Per-book settings (Angle, zooms, reading order, screens, Skip empty, animations) are kept in the spare bytes of the book's `.sav` state; older `.sav` files read as the defaults.
- The converter reads PDF (pypdfium2, or pdftoppm), EPUB (its pictures in reading order), CBZ, picture folders and pictures. Zooms: any of 1.1–3.0 for each angle (steps of 0.1; Fit page and Fit width are always there, 1.0 is no longer a tick box); defaults 1.6/1.8/2.0 and 2.2/2.4/2.6. The reader takes up to 48 zooms (16 before; a file with more needs this build). Ready-made programs with a window for Windows (`gbcomic.exe`) and Linux (`gbcomic`): comics, angles, zoom tick boxes, a preview of a page at a zoom on the GBA screen (240 × 160, pages, A/B, D-pad and mouse, page map; made by the converter's own code and read with the reader's rules, checked screen for screen against the reader), picture options, where to save, a size estimate and progress. Other options: dithering, sharpening, lettering boost, margins, spreads, PDF resolution. Format: `docs/gbcomic-format.md`.
- Sleep on the book list: `L+R` turns the screen off and stops the GBA (almost no power). `Start` held 1 s wakes it, back on the same list; a shorter press (or `Start` let go for an instant) leaves it asleep with the screen off, and the waking press does nothing else.

## Not read any more

- `.gbcomic` files from the earlier V3.3 test builds (version 1): `Old .gbcomic: convert again`.
- CBZ files are not listed or opened; convert them with the gbcomic conversion software ([halimj.itch.io](https://halimj.itch.io/)).
- EPUBs are always read as text.

## Planned

- PDF.

## Sizes and speed (measured)

- A 25-page colour comic (from a PDF at 200 dpi) became 29.7 MB with the defaults (about 1.2 MB per page: upright 1.6x, 1.8x, 2x about 580 KB, sideways 2.2x, 2.4x, 2.6x about 480 KB, Fit page and Fit width the rest).
- In mGBA with a modeled Supercard (its card timing is synthetic): the next screen of a page in 1 frame (loaded ahead while waiting), a new page in 7 frames; D-pad scrolling and A/B slides at 60 frames a second, upright and sideways. Comic reads go straight to the card (one read per run of chunks), about three times faster than through the text reader's windows.

## Tests

- Host tests: the converter's files read back with the reader's code (every screen of every zoom equal to the stored picture, ten zooms, three reading orders, even spreading, spreads, ink grids, old/newer/damaged files refused); the screen layout (counts, reading order, ink cells); the A/B rules after D-pad moves (edges, heights, right to left, Columns); and the comic on screen simulated on two screen pages for every zoom and all four angles with random jumps, slides and D-pad moves, every frame equal to the screen drawn whole (six runs, about 47,000 frames); per-book settings records, turned keys and Start on release, `.gbcomic` listed and `.cbz` not; presses and Settings during slides in the simulated comic (the same screens as presses after each slide, every frame equal to the screen drawn whole); the key latch (300 presses 2-6 frames apart in a loop taking 6 frames: all counted, in order; `Start` let go, combos); `B` presses while Back loads; all pass, also with AddressSanitizer and UBSan.
- The ROM ran on mGBA with a modeled Supercard SD card: opening, zooms upright and sideways, A/B jumps and Smooth slides, D-pad scrolling to the edges then A/B to the next row, the page map, the View screen with columns and rows, the Animations screen, Settings rows, saving. Not physical hardware proof.
- `tests/emu/comic_test.py` (exact ROM, modeled Supercard, also in CI): a detailed generated comic read with quick `A` / `B` presses on a normal and a slow card (frames that do not move while sliding below a limit, the screens those of slow presses, none lost), Settings during a slide at 90° right and `L` held during a slide (the same screen as after a finished slide), and quick page turns in a 4 MB TXT (the page of slow presses). Every screen walked with the smaller Butano tables (Home, Import, Settings, About, View, Animations, Angle, turned screens, save overlays, comics with the page map, Arabic text) without a Butano error.
- `tests/emu/sleep_test.py` (exact ROM, modeled SuperCard, Omega with and without the kernel IRQ hook, EverDrive): asleep the CPU runs no code and the screen stays blank; `Start` for 30 frames, or let go for one frame during the hold, stops it again; held 60 frames it wakes on the same screen pixel for pixel, with no Import; `Start` afterwards works as usual.

Physical hardware has not been tested with this version.

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
