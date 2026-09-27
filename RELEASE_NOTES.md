# gbareader V1.9 (pre-release)

Based on V1.8 (`v1.8.0`). Reading, storage (`.sav` companions) and controls are unchanged.

## No limit on the number of books

- Home now lists every supported TXT/EPUB file in `/gbareader`. Earlier versions stopped at 64 files.
- Names are kept in RAM 64 at a time (the same memory as before) and reread from the folder when you move past them: about one folder read per 32 books scrolled. Books stay in folder order.
- If the folder cannot be read while scrolling, the row shows `?` instead of a wrong name.

Physical hardware has not been tested with this version.
