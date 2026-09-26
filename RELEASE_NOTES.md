# gbareader V1.6

Based on V1.5 release `b7d51688c810150d9274e878825e0bdf28ca5bd2`. Only reader settings, page layout, page numbering, version/help and instructions change. This is a new release; main and all historical releases/tags/assets are preserved.

## Reader settings

- **Line spacing: 0 to 4** pixels (was 1 to 4). 0 fits ten lines; the default 1 still fits nine.
- **Paragraph gap: None, Small, Half or Full.** Full is one empty line, exactly as before, and is the default. Smaller gaps fit more text on pages with many paragraphs.
- **Top and bottom margin are removed.** They rarely changed the number of lines. Text is now centered vertically instead of leaving a blank strip at the bottom.
- **Lines per page** is shown and updates as you change the settings.
- **Go to: percentage.** Left/Right change it by 1%, L/R by 10%. Press A to jump straight away (closing settings with B or Start also jumps if the value changed); the reader jumps to the start of that paragraph (or the next word inside a very long paragraph).
- **Page number and percentage**, for example `Page 142 - 37%`. Pages are counted in the background from the start of the book, one page per idle reading frame and never in the same frame as Back-history rebuilding, so it shows `Page ...` for a while after opening, jumping or changing settings.

With default settings the page looks as before, moved down by a few pixels.

## Files and preservation

Put supported UTF-8 TXT and text-only, DRM-free EPUB books directly in `/gbareader` at the SD-card root. The ROM remains `gbareader.gba`.

V1.4 storage is unchanged: books are opened read-only, and state and EPUB cache stay in `Title.txt.sav` / `Title.epub.sav`. The new settings are stored in the same state fields: the former top-margin byte holds the paragraph gap, and display layout 4 marks the new meaning. Books saved by earlier versions keep their reading position, reset to the default settings, and rebuild Back history once.

## Verification

- Full host and ASan/UBSan suites pass. Both sector-backed FatFS fixture suites and the SAV companion suite pass.
- New tests cover every spacing and gap value, lines per page and centering, save round trips and rejection of invalid values, older-layout saves, background page numbering against sequential paging, percentage snapping (paragraph start, long paragraph, UTF-8 boundaries, end of book) and page-turn counting in the extracted production input code.
- The ROM is built by the repository's GitHub Actions workflow. It has not been tested on physical Supercard/SD hardware, and background page-count speed has not been measured.

Keep backups and never remove the card during storage operations.

## Downloads

- `gbareader.gba`: runnable ROM.
- `gbareader-full-controls.pdf`: complete controls and instructions, credited to Halim Jarrar.
- `gbareader-full-controls.md`: the same complete instructions as text.
- `SHA256SUMS`: checksums for the three files above.
