# gbareader V1.3 — pre-release

Read TXT and EPUB books on your Game Boy Advance. Save your place and return to it later.

**Experimental pre-release, on `prerelease/v1.3.0`; not merged into `main` and not designated the latest stable release.**

## Changes

- In-app title: `gbareader V1.3`.
- Arabic shaping starts OFF for a document unless that document already has a saved ON flag.
- Opening an OFF/unmarked document checks only the page about to be displayed: the saved reading position, or the beginning. Finding Arabic enables shaping and automatically saves sticky ON inside the document's existing state.
- Future opens of that document retain ON regardless of which page is displayed. Other files are unaffected.
- OFF sessions bypass subsequent Arabic detection and shaping. The existing Arabic shaper and native fonts are retained when ON; this is not a rewrite of the shaping algorithm.
- Saved byte positions remain intact. Incompatible page history is rebuilt when layout mode changes rather than reusing mismatched page boundaries.
- No `.sav`, permanent sidecar, new typing mode or page-turn control is introduced. Controls and the full-controls PDF explain automatic activation saving.

## Intentional limitation

An English-only opening/resume page leaves shaping OFF even if Arabic exists elsewhere in the book. Arabic encountered later in that session is not shaped. Save on a page containing Arabic and reopen there to activate it. Once enabled and successfully saved, the flag remains ON for that file; there is no new OFF toggle.

## Downloads and saving

- `gbareader.gba` — complete runnable ROM.
- `gbareader-full-controls.pdf` — complete controls, saving behavior and credits; also in `docs/`.

Create `/gbareader` at the SD-card root and place supported UTF-8 TXT/text-only EPUB books directly inside. Keep backups of your books. Initial EPUB cache preparation and first-time Arabic activation write state automatically. Wait for writes to finish. If activation saving fails, shaping can remain ON in RAM without being stored for the next open; keep the book open and retry with Reader Start. Later position/settings changes still require Reader Start before closing with Select.

## Verification status

Full host tests, ASan/UBSan, clean devkitARM ROM build and static memory/frame budgets passed. Normal and legacy sector-backed FatFS suites passed successful-save and exercised transient-recovery gates, plus production opening-mode checks for TXT, original/cached EPUB, sticky resumes, state-only updates, failed-write retry and original-content preservation. Independent review found no demonstrated data-integrity or memory-safety blocker; its opening-boundary finding was corrected and regression-tested so only accepted opening-page text triggers activation.

These automated checks are not physical Supercard tests or performance measurements. Exact-ROM emulator scope is reported separately in the GitHub pre-release description. The downloadable ROM retains the frozen build SHA256 `d76dfb5b0e3a0a46e4db24101946ea08129446798acc34ad20812077dd88c4b2`.

Inherited fault-test limitations remain: persistent-error/abrupt-interruption cases can lose TXT bookmarks, and three second-sync fault selectors are not reached. Static memory/frame budgets are not exhaustive call-tree proofs.

Physical Supercard/SD operation is a separate hardware check. Persistent storage errors and arbitrary power loss can lose bookmarks under inherited save semantics. EPUB remains bounded and text-only, with no DRM/images/CSS/embedded-font presentation. Compaction is deferred and saves remain subject to the inherited archive size limit. Arabic follows the existing bounded per-line direction policy, not full Unicode bidi or complete Persian/Urdu coverage.
