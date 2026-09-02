# GBA Reader v0.4.7 — Faster saved EPUBs and publishing glyphs

- The first successful save of an EPUB now appends a GBAReader-only normalized-text cache after the untouched original ZIP bytes.
- A duplicate central directory and fresh ZIP end record follow the cache, keeping the resulting file readable by ordinary EPUB/ZIP software even when cached text exceeds 64 KiB.
- Reopening a cached EPUB sequentially CRC-verifies the compact text, skips ZIP package discovery and the full count-only XHTML inflate/parse pass, then reads the cached text directly with the existing 512-byte file buffer.
- Subsequent saves replace only the fixed bookmark footer, so the cache and file size do not grow repeatedly.
- Failed first-cache writes or syncs restore the previous footer and exact prior file length.
- Cache text is CRC-verified before activation. Corrupt cache data or invalid cache metadata disables the fast path and falls back to the intact EPUB ZIP structure.
- A 3,300-byte supplemental SuperFW pack adds common Unicode publishing punctuation, currency and letterlike symbols, including smart quotes, en/em dashes, bullets, ellipsis, `€` and `™`.
- Sparse font entries now fall back safely instead of indexing invalid glyph data.
- Unicode minus and full-width ASCII characters receive readable ASCII fallbacks.
- A controlled synthetic TXT/EPUB corpus verifies direct UTF-8 and entity-derived glyph coverage while retaining an intentional unsupported-emoji case.
- Common named EPUB entities for euro, pound, yen, bullet and middle dot now decode to their real Unicode characters.
- Existing v1/v2 bookmarks remain compatible, and the original EPUB ZIP bytes are never rewritten.
