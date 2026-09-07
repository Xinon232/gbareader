# GBA Reader v0.5.0 — Valid EPUB cache members and resumable saves

- EPUB normalized text caches are stored ZIP members at `META-INF/gbareader/cache-v5`; no proprietary bytes follow the final EOCD.
- The original ZIP entries (including `mimetype`) remain intact and the resulting file opens in ordinary ZIP/EPUB readers.
- Cache metadata carries a processor version, source-size provenance, normalized-text CRC and a fixed text processor version. Cached reads use the existing bounded window rather than a whole-book allocation.
- Cache creation is transactional on first save; later EPUB saves update state without rebuilding normalized text.
- TXT saves now write fixed ASCII v3 footers. v1 and v2 remain readable and migrate on the next save.
- A saved v3 Back-history rebuild deliberately restarts from byte zero at its anchor after reload; it never resumes an incomplete, unserialized page scan.
- A bounded v0.4.7 EPUB trailer/footer detector opens the original pre-trailer archive and legacy bookmark when validation succeeds; the next successful save migrates it to v0.5 ZIP members.
- Pagination no longer fetches the first UTF-8 byte twice.
- The release workflow builds `gbareader.gba`, uploads it as `GBAReader-v0.5.0`, and attaches it to the v0.5.0 GitHub release.
