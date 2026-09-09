# gbareader v1.1 — five-book home

Read TXT and EPUB books on your Game Boy Advance. Save your place and return to it later.

## What changed

- Home shows five books at once instead of four. The list moves slightly up and navigation instructions move down, leaving Controls and Credits visible at the bottom.
- The title is `gbareader V1.1`. Title/path placement, fonts, button actions, folder discovery, the 64-file index and 255-byte basename limit are unchanged.
- Parser, cache, pagination, history, settings and embedded-save algorithms are unchanged from v1.0.0. This is a scoped Home layout release, not a storage or reading-engine update.
- The complete controls PDF is updated for V1.1 and the five-book display.

## Downloads and setup

Download **gbareader.gba** and **gbareader-full-controls.pdf**. Create `/gbareader` at the root of your Supercard SD card and put supported UTF-8 `.txt` and text-only `.epub` books directly inside, then restart the app. Keep backups before using embedded saves.

Reader `Start` saves position, settings and Back history inside the open book; Reader `Select` closes without saving. Initial EPUB cache preparation writes initial state automatically; later position/history/settings changes require a successful Reader `Start` save. TXT uses a trailing bookmark footer; EPUB uses internal ZIP cache/state members, not permanent `.sav` or settings sidecars.

## Verification and known limitations

**V1.1 is a final release. Physical Supercard/SD hardware remains unverified.** Final release status does not imply hardware or power-loss certification.

- A clean ROM build and the full host and ASan/UBSan suites passed. New tests cover five-row selection windows through the inherited 64-file limit and the actual Home bitmap-render branch using shipped fonts, including long names, empty/unavailable storage and error presentation.
- The exact ROM passed an unpatched, unseeded emulator cold boot, all five Controls pages, Credits, boundaries and repeated returns. Home path and navigation/footer pixels were compared with the inherited ROM at their unchanged or requested shifted positions.
- A separate exact-ROM emulator run used a clearly labeled RAM-only seven-book fixture, not a mounted SD card. Real keypad events exercised every selection, scrolling in both directions, both list boundaries and both footer controls. Complete screen pixels matched the real-font expected five-row bitmap and cold-boot UI. No ROM patches, save commands or program-counter/stack-pointer relocation were used.
- Both PDF pages were rendered and visually inspected. The complete README controls round-trip into the PDF; the unchanged settings/saving/compatibility/credits page retains its text. Regenerating unchanged documentation produces identical PDF bytes.
- Physical SD discovery/open/save behavior was not retested in an emulator; the storage code is unchanged. Retained v1.0.0 FatFS image evidence is historical, not a new hardware claim.
- Inherited TXT limitations remain: persistent write failure or interrupted footer replacement can lose an old-or-new bookmark. Arbitrary power loss, torn sectors, out-of-space conditions and exhaustive faults remain unverified. These are not fixed by v1.1.
- EPUB remains a bounded text-only subset without DRM, images, CSS presentation or Arabic support. Physical EPUB compaction remains deferred; appended saves remain subject to the inherited 128 MiB archive limit.

The release ROM should be the exact emulator-verified `gbareader.gba` below, not a replacement CI build. Its visible title is `gbareader V1.1`; the inherited ROM header is `GBA READER`. CI status and publication verification are separate from local exact-ROM verification.

## SHA-256

```text
adc7a0eaf1d9b53e26a9f2efd5163ad5e5e0e7b33bc530f32db1e3e23c99a188  gbareader.gba
14dcd07543ee0c1e5a406d9e84658ae8da96011783cd64b44571f7e13f4782bf  gbareader-full-controls.pdf
```

Made by Halim Jarrar. Existing GPL and third-party notices remain in the repository.
