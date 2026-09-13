# gbareader V1.5 — final

Based on V1.4 sidecar release `5947a99ea9a40456e02f7307c05fe10041eee6b5`. Only reader hold controls, their notice, version/help and instructions change. This is a new final/latest release; main and all historical releases/tags/assets are preserved.

## Controls

- While reading, hold **Up alone** to toggle the existing shoulder page-turn mode; hold **Down alone** to open reader settings.
- Both require **48 held updates after the initial press**, exactly matching R-only Caps in current gbawriter V1.2 (`577ff2a88b80cd8673672fd16e8629649678fca5`, `InputState::CAPS_HOLD_DELAY = 2 * NAV_REPEAT_DELAY`, where the repeat delay is 24). At nominal GBA cadence this is about 0.8 seconds.
- Short presses do nothing. A continuous hold acts once. Another key or leaving reading cancels it; release and press again to retry.
- `L+R: On` / `L+R: Off` briefly appears in the foreground UI font at the top-right. The non-blocking notice lasts 60 application frames, about one second. When enabled, L goes back and R advances. Mode still starts OFF each app launch and is not saved.
- Library/settings Up/Down remain immediate. All other controls are unchanged. In-app Controls now has nine complete pages.

## Files and preservation

Put supported UTF-8 TXT and text-only, DRM-free EPUB books directly in `/gbareader` at the SD-card root. The ROM remains `gbareader.gba`.

V1.4 storage is unchanged: books are opened read-only. State and EPUB cache stay in the full-filename companions `Title.txt.sav` / `Title.epub.sav`. Copy, rename and back up each book together with its companion. Existing migration, recovery, cache, Arabic, byte anchors, layout, fonts and history behavior remain intact. Full details and inherited limitations are in the controls manual and unchanged `sidecar-format.md`.

## Verification

- Vertical failing-then-passing tests exercise extracted production keypad sampling, dispatch, timer and render calls, including all short durations, exact threshold, long holds, rearming, every companion and scene interruption, notification reset/expiry and unchanged page/save actions.
- Full host and ASan/UBSan suites pass. Both actual sector-backed FatFS fixture suites pass their successful-transaction and exercised transient-error gates, including read-only source-byte checks. Unhit fault selectors remain explicitly reported, not counted as tested.
- Clean native devkitARM build. Unchanged gates: EWRAM 147328 bytes, IWRAM 9528 bytes, largest runtime stack frame 6736 bytes. The bounded keypad sampler uses a 24-byte frame; this is not a whole-call-tree or physical stack proof.
- Exact release ROM exercised with libmGBA: cold boot V1.5; all nine Controls and seven Credits pages; short/long Up/Down; notice On/Off/expiry; interruption and settings/Home return. Reading uses one explicitly RAM-seeded synthetic page, followed by real emulated keypad input. No ROM/PC/SP/register patching or physical SD claim.
- Complete four-page controls PDF: deterministic rebuild, Markdown/PDF text parity and every page visually inspected. Independent read-only review found no blocking defect.

Physical Supercard/SD operation, arbitrary electrical power loss and exhaustive media-failure recovery are not claimed. Keep backups and never remove the card during storage operations.

## Downloads

- `gbareader.gba`: runnable ROM.
- `gbareader-full-controls.pdf`: complete controls and instructions, credited to Halim Jarrar.
- `gbareader-full-controls.md`: the same complete instructions as text.
- `sidecar-format.md`: unchanged V1.4 SAV specification.
- `SHA256SUMS`: checksums for the four files above.
