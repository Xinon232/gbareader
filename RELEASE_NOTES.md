# GBA Reader v0.2.1 release notes

GBA Reader v0.2.1 fixes EPUB discovery in the SD-root library and expands FAT
long-filename handling to 255 bytes. The library now indexes up to 64 `.txt` and
`.epub` books while keeping only four rows visible at once to remain within the
GBA sprite limit.

Reading positions are now stored independently for up to 32 filenames. Position
changes are saved automatically when a book opens, a page changes, settings
change or the reader closes. Pressing `R` performs a manual save. Existing
v0.1.0/v0.2.0 SRAM state migrates automatically.

Line spacing, top margin and bottom margin now each use a clear 1–4 range. Runs
of three or more ordinary spaces collapse to one space, and repeated CR/LF line
breaks collapse to a single line break.

Direct text-only EPUB support from v0.2.0 remains bounded and read-only. ZIP64,
multi-disk archives, encryption/DRM, unsupported compression, Arabic, images,
CSS presentation, JavaScript, embedded fonts, audio, video and SVG remain
unsupported. See README for complete limits and compatibility details.
