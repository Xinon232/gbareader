# GBA Reader v0.4.5 — Embedded EPUB saves and larger books

- `Start` now embeds the same checksummed progress/settings footer in TXT and EPUB books. Recognizable footers are hidden from both logical streams even after an interrupted write, while progress is restored only from a checksum-valid footer; EPUB parsing sees the original ZIP length and repeat saves replace rather than append.
- Save completion is checked and reported honestly as `Saved` or `Save failed` after the write attempt.
- Streaming XHTML support increases to 32 MiB uncompressed and 16 MiB compressed per required entry, with a 128 MiB archive bound and no whole-chapter allocation.
- Archive, metadata, compressed-entry, chapter-count, chapter-size and total-text limits now have distinct diagnostics.
- Exact bookmark restore and settings close no longer rebuild page history from byte zero. Back history is reconstructed lazily, and the fixed 64-page history now uses a circular buffer.
- Existing TXT footer compatibility, ZIP/metadata safety bounds, and the text-only EPUB scope are preserved.
