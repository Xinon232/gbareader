# GBA Reader v0.4.6 — Immediate Back navigation and transient save status

- Current-format embedded TXT/EPUB bookmarks now preserve the fixed 64-page circular Back history in addition to reading position and layout settings.
- Reopening a newly saved book makes up to 64 previous pages immediately available without scanning from byte zero.
- Existing 96-byte v1 bookmarks remain compatible. Missing Back history is rebuilt one page per main-loop update; an early Back request shows `Loading back...` instead of blocking in an apparent freeze.
- `Saved` and `Save failed` now disappear automatically after a short non-blocking interval, while page and menu controls remain responsive.
- The new checksummed 384-byte v2 footer replaces either an existing v1 or v2 footer without cumulative file growth.
- Failed footer upgrades restore the exact previous footer and file length.
- TXT and EPUB logical streams continue to hide recognizable save footers, including checksum-invalid interrupted saves.
