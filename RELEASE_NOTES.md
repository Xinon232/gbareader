# gbareader V1.7

Based on V1.6 (`v1.6.0`, commit `8428a14`). Fixes slow Back and missing page numbers after Go to, and replaces the Butano UI font with the gbamp3 5x7 font. Book text, storage (`.sav` companions) and controls are unchanged; V1.6 saves open as before.

## Fixes

- **Back after Go to is quick.** Back history used to be rebuilt by laying out every page from the start of the book, so Back right after a deep Go to (or after resuming deep in a book) could show `Loading back...` for a minute or more. It is now rebuilt from a line start about 8 KiB before the page, and the next older pages load ahead while you page back. Page layout is also about twice as fast (cached character widths). In mGBA with a 1 MB book: Go to 50% and Back at once took about 62 s before, about 0.5 s now.
- **Page number after Go to.** Settings used to stay on `Page ...` after a jump. Page counting now also runs while Settings is open, shows an estimate such as `Page about 1500` while it counts, and the exact number when done. A Go to with unchanged settings keeps the pages already counted, so jumping back and forth is quick.

## Look

- **UI font: the 5x7 font from gbamp3** (v0.9.8) for titles, settings and key hints (hints in gbamp3 grey). Messages over book text (`save...`, `Loading back...`, `L+R: On`) sit on a white box, like gbamp3 messages.
- **The blue `>` cursor** (Butano font) stays in Settings and now also marks the selected book on Home. Settings rows share one left edge.
- Book text, file names and the Controls/Credits pages keep the SuperFW font. Credits name the new UI font.

The ROM is `gbareader.gba`. Put books in `/gbareader` at the SD-card root, as before.
