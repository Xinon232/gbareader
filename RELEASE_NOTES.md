# gbareader V1.8

Based on V1.7 (`v1.7.0`). Opening EPUBs is faster; reading, storage (`.sav` companions) and controls are unchanged. V1.7 companions and caches work as they are.

## Faster reopening of cached EPUBs

- When an EPUB already has its `.sav` cache, gbareader no longer re-checks every file inside the ZIP or re-reads the book's chapter list before showing the text. Opening the companion already proves the book is unchanged (size, sampled bytes and a fingerprint of the ZIP directory, which includes every file's checksum), so only the small cache table is read.
- The full checks still run whenever there is no valid cache. If a cached text block ever fails its checksum, the original chapters are checked and used, exactly as before.
- Measured on a FAT32 disk image with a 1.9 MB, 200-chapter EPUB: the EPUB part of a cached reopen went from 668 disk reads (9,728 sectors, about 4.9 MB) to 6 reads (6 sectors).

## Less reading on the first open

- Isolated lookups (a ZIP file header, one directory entry) now read a single 512-byte sector instead of refilling an 8 KiB window; sequential reading still uses the 8 KiB window.
- Same book, first open: 636 disk reads / 7,146 sectors instead of 1,148 / 17,498.

These are disk-image measurements; real Supercard SD timings have not been measured.
