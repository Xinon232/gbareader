# v0.2.3 file-I/O architecture

## Scope

This release keeps the existing bounded streaming architecture and dict.cc-compatible five-field format. It does not store complete imported files in RAM and does not change controls, fonts, rendering, shuffle/undo semantics, or Leitner rules.

## Read path

- Initial and post-save indexing share one sequential scanner with one aligned 512-byte EWRAM refill buffer.
- Indexing uses a lightweight raw structural validator: raw-row length, first-tab split, CR/space/tab trimming, and nonempty sides only. It does not invoke UTF-8 conversion, font mapping, Arabic joining/shaping, or visual-order conversion.
- Display still uses `parse_line_into()` and the existing multilingual conversion/render path.
- A single current-card cache is keyed by loaded-file generation, card-array generation, and physical row offset. Field-only changes refresh `LineBuf::field` without rereading text.
- A card miss opens the source, seeks once, performs one bounded read, and closes it. The same bounded reader is reused by save.

## Save path

- START returns success immediately when neither dirty fields nor card-array ordering changed; it performs no rewrite or reindex.
- Changed saves remain full streamed grouped rewrites. A 512-byte EWRAM output buffer emits raw row bytes plus CRLF and verifies every write result and byte count.
- Output remains fields 1 through 5, original relative order within each field, CRLF rows, and one blank CRLF separator between fields.
- Dirty bits are cleared only after temporary write/sync/close, replacement, and structural reindex all succeed.
- A bounded 25,904-byte EWRAM metadata scratch permits reindex failure without destroying live fields or dirty state. It contains offsets/fields/counts/dirty metadata, never complete vocabulary text.

## Replacement transaction and recovery

Sidecars replace the `.txt` suffix rather than appending it, so a maximum 63-byte app filename remains within `VOCAB_FILENAME_MAX` and FatFS limits:

1. Refuse ambiguous sidecars unless recovery can prove the original structurally valid.
2. Write `name.tmp` completely, `f_sync()` it, and close both files successfully.
3. Rename `name.txt` to `name.bak`.
4. Rename `name.tmp` to `name.txt`.
5. Reindex the new original with the shared scanner into metadata scratch.
6. Delete `name.bak` only after successful reindex.

Failure handling:

- Before step 3, the original remains authoritative.
- If step 4 fails, rename the backup back to the original.
- If reindex fails, park the new original as `.tmp` and restore `.bak` to `.txt`; dirty state remains live.
- If power is lost with only `.bak` plus `.tmp`, load validates `.bak` and restores it.
- If `.txt` plus `.bak` exist, load validates `.txt` before removing `.bak`; if `.txt` is invalid and `.tmp` is free, it parks `.txt` and restores the valid backup.
- A stale `.tmp` is deleted only after the original passes structural indexing.
- The only valid copy is never unlinked merely to force progress. Ambiguous states fail conservatively.

This is a recoverable transaction design, not a claim of guaranteed power-loss safety on untested flash hardware.

## Deliberately disabled evaluations

### Persistent source handle

Not retained. The current-card cache removes all unchanged-frame opens, so a persistent `FIL` would benefit transitions only while adding close/reopen obligations across save, rename, file switch, remount, media errors, and recovery. With `FF_FS_TINY=0`, it also reserves another 512-byte per-file FatFS buffer. Actual Supercard timing is required before accepting that complexity.

### FatFS fast seek

`FF_USE_FASTSEEK` remains `0`. A bounded CLMT can accelerate fragmented-file seeks, but sizing it safely requires real FAT32/exFAT fragmentation data. A too-small map falls back cleanly in principle, but persistent CLMT RAM and rebuild/recovery paths are not justified without hardware measurements. No CLMT RAM is allocated in v0.2.3.

### Supercard fast ROM mirror

`SC_FAST_ROM_MIRROR` remains default `0`. It is exposed as an opt-in build setting:

```sh
make -j2 SC_FAST_ROM_MIRROR=1 LIBBUTANO=/home/hlm/butano/butano
```

The optional build path is compile-checked only. Real Supercard/SuperFW verification is required; no transfer-speed or reliability claim is made.

## FatFS configuration

- exFAT and UTF-8 LFN support remain enabled.
- `FF_MAX_LFN` and `FF_LFN_BUF` are reduced from 255 to 63, matching the app's 64-byte filename buffer. The documented stack formula drops from 1,120 to 352 bytes for affected LFN calls (768-byte reduction).
- `FILINFO.fname` drops from 256 to 64 bytes (192-byte per-instance reduction).
- Unused `FF_USE_EXPAND` and `FF_USE_CHMOD` are disabled.
- Both project `ffconf.h` copies remain byte-identical.

## Deterministic host profiles

- 5,000-card, 145,000-byte index: 145,000 legacy one-byte reads versus 284 512-byte block reads.
- 121 renders of one unchanged card: 121 legacy read/parse cycles versus one initial bounded read/full parse and zero additional reads/parses for 120 repeats.
- 150,008-byte grouped save: 10,004 legacy small writes versus 293 buffered writes.

These are deterministic call-count tests, not SD-card timing claims.
