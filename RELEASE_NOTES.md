# gbareader v1.0.0 — home library and controls

Read TXT and EPUB books on your Game Boy Advance. Save your place and return to it later.

## What changed

- Home lists supported UTF-8 `.txt` and text-only `.epub` books directly from `/gbareader` at the SD-card root. There is no intermediate load menu, card-wide browser or root-folder fallback.
- Home shows `gbareader V1.0`, the folder path, and accessible Controls and Credits, including when the library is empty or unavailable.
- Five in-app Controls pages and the attached full-controls PDF explain file placement, reading, settings and saving. This reader has no typing, editing or user-facing export controls.
- The v0.8.0 parser, block cache, pagination, history, settings and embedded-save algorithms are retained; storage changes are limited to library discovery and complete folder-prefixed paths. The inherited 64-file index and 255-byte basename limits are implementation limits, not SD-card capacity limits.

## Downloads and setup

Download **gbareader.gba** and **gbareader-full-controls.pdf** below. Create `/gbareader` at the root of your Supercard SD card and put supported books directly inside, then restart the app. Keep backups before using embedded saves.

Reader `Start` saves position, settings and Back history inside the open book; Reader `Select` closes without saving. Initial EPUB cache preparation writes initial state automatically; later position/history/settings changes require a successful Reader `Start` save. TXT uses a trailing bookmark footer; EPUB uses internal ZIP cache/state members, not permanent `.sav` or settings sidecars.

## Verification and known limitations

**This is a hardware-unverified prerelease. No physical Supercard/SD test was performed.** Emulator results do not prove physical storage compatibility or power-loss safety.

- The exact attached ROM passed 29 unpatched, unseeded emulator checkpoints covering home, all five Controls pages, Credits, boundaries and repeated returns. Its unavailable-storage home was tested, not a real mounted SD folder.
- Host and ASan/UBSan regression suites passed, including real-font framebuffer checks and scoped library/path tests. Production FatFS image tests exercised successful saves and transient-error recovery; long ASCII and 255-byte BMP UTF-8 TXT/EPUB basenames were verified with fresh remounts.
- Inherited TXT limitations remain: persistent write failure and interrupted footer replacement can lose a valid old-or-new bookmark. The exercised cases preserved original text/body and clean filesystem checks, but bookmark recovery is not guaranteed. These are not fixed by v1.0.0.
- Three selected sync-error ordinals in the retained FAT fault campaign were not reached; the campaign is not 39 clean passes. Exhaustive faults, torn sectors, out-of-space, arbitrary power loss and physical hardware behavior remain unverified. Supplementary-plane FAT filenames remain unverified because fixture tooling could not create the intended name.
- EPUB remains a bounded text-only subset, not full EPUB support. Physical EPUB compaction is deferred; appended saves remain subject to the inherited 128 MiB archive limit.

The supplied ROM is the exact frozen emulator-verified artifact, not a replacement CI build. Its visible title is `gbareader V1.0`; the inherited ROM header is `GBA READER`. The Makefile's v0.8.0 comment describes inherited build metadata, not the published release version. CI results for the final commit are available in the repository Actions tab and are separate from exact-ROM verification.

## SHA-256

```text
b2d53dd0096f1fc89b3893057806e6a85ac9c016e8a88c3a380d07f762077e73  gbareader.gba
98f735a63883463f5bfb2fec1dc316bf4854639637ae7ed83e3db3058666b514  gbareader-full-controls.pdf
```

Made by Halim Jarrar. Existing GPL and third-party notices remain in the repository.
