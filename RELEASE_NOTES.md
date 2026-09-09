# gbareader V1.2 — final release

Read TXT and EPUB books on your Game Boy Advance. Save your place and return to it later.

## Changes

- Native Ghoulam Arabic contextual joining and lam-alef, RTL line display and matching shaped wrapping. Harakat are hidden only on screen; source bytes and bookmark anchors remain intact.
- Latin, digits, Korean/Hangul, CJK and inherited fonts/pixels stay unchanged. Arabic uses 11px native artwork within existing 16px rows. Mixed direction is bounded, not full UAX #9 or full Persian/Urdu coverage.
- The existing checksummed display-layout marker advances to 2. Old/unknown Back history rebuilds from the unchanged saved byte anchor; compatible current history remains reusable. EPUB parser/cache and file save algorithms are unchanged.
- In-ROM title `gbareader V1.2`; six Controls pages and seven Credits pages. Credits page one is the exact personal block only. Subsequent pages credit Ghoulam, SuperFW, UNSCII, Unifont/Hangul, Butano, FatFs, miniz and the toolchain/base project. Left/Right pages Credits, B/Start closes; reading/save controls are unchanged.
- Bounded malformed-body fallback avoids forwarding truncated UTF-8 to the inherited renderer, which can otherwise read beyond NUL. Valid non-Arabic rendering is unchanged.
- Complete three-page controls PDF includes files, all controls, automatic initial cache writes, explicit saves, compatibility, Arabic limitations and source/license links.

## Release files

- `gbareader.gba` (stable required basename)
- `gbareader-full-controls.pdf` (also in the repository under `docs/`)

Create `/gbareader` at the SD-card root and place supported UTF-8 TXT/text-only EPUB books directly inside. No demos or test fixtures belong in release data. Keep backups of your books. Reader Start saves; Reader Select closes without saving. Initial EPUB cache preparation writes initial state automatically. No permanent `.sav` or settings sidecar is introduced.

## Verification and publication status

**Final V1.2 release (`v1.2.0`). Independent source review, host verification and exact-ROM emulator QA passed.** Historical releases, tags and assets are preserved. The downloadable ROM is the exact emulator-verified build, not a replacement CI artifact.

Host tests and independent ASan/UBSan verification passed for native glyph pixels/bounds, joining/reference glyph IDs, long multiline Arabic TXT and original/reopened cached EPUB, mixed Latin/digits/full-width text, forward/back/resume, old/current bookmarks, unchanged source bytes, controls/credits framebuffers and storage regressions. The separate offline HarfBuzz comparison passed all 1504 cases. Real-FatFS image tests passed their successful-save and exercised transient-recovery gates, retaining inherited bookmark-loss and unhit-fault-selector limitations.

The exact V1.2 ROM passed actual emulator cold boot, all seven Credits pages, all six Controls pages, boundaries and close/reopen checks. Arabic presentation, forward/Back pagination and byte-anchor resume/rebuild passed with RAM-seeded TXT-source and already-normalized EPUB text windows. These body checks do not exercise mounted storage, ZIP parsing, DEFLATE, XHTML normalization, cache creation/reopening or persisted bookmarks; original/cached EPUB and old/current saved-marker coverage comes from host tests, not emulator storage. No crash or visible clipping was observed in the inspected samples. Broader mixed-direction linguistic review remains appropriate.

Release asset SHA256:

```text
eb4519e3aa06e235bdd1e84d67fd5a3e58fa43dc345f3dc4b3c0698cfd5fbc95  gbareader.gba
ae0020219d246d96c388edfcb4e7f27d901901119107e668405fe8cbe2b8d368  gbareader-full-controls.pdf
```

Physical Supercard/SD operation remains unverified. Persistent storage errors and arbitrary power loss can lose bookmarks under inherited save semantics. EPUB remains bounded and text-only, with no DRM/images/CSS/embedded-font presentation. Compaction is deferred; saves remain subject to the inherited archive size limit.

The exact inherited UNSCII/Unifont font snapshot versions are not documented; conservative GPL attribution is retained without claiming newer OFL eligibility. See [Arabic and credits audit](docs/arabic.md).

Made by Halim Jarrar · (C) 2026 · halim-jarrar.de · monday@halim-jarrar.de
