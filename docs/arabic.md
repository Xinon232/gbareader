# Arabic display — gbareader V1.3 pre-release

Made by Halim Jarrar · (C) 2026 · halim-jarrar.de · monday@halim-jarrar.de

## Font provenance

Ghoulam Regular © 2025 Imad AlFil / mloukhiyye, CC BY 4.0.
- Source: https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1
- License: https://creativecommons.org/licenses/by/4.0/
- Original supplied TTF: `tests/fixtures/arabic/Ghoulam-Regular.ttf`, SHA256 `8c0e0ce06244dc1234e27d22170e05bb8a92b45169d61ffe8c74e7b3a31717aa`.
- Converted by `tools/extract_ghoulam.py` with fontTools and FreeType: actual unencoded GSUB initial/medial/final forms and required lam-alef ligatures, native 11px, baseline 11. This modified monochrome representation preserves advances and bearings; no author endorsement is implied.
- Table and shaper imported from gbavocab commit `03d8eade4dfa2fcc5a088c309e3463ca7cc15f74`. Generated font bytes were regenerated and compared exactly. The supplied TTF has not been established byte-identical to a fresh upstream download.

## Per-document activation

A missing or OFF flag starts the document with shaping off. Opening checks only the provisional page at the saved byte anchor (or the beginning); finding Arabic enables shaping, re-lays out that page at the same anchor and automatically saves sticky ON inside the existing TXT footer or EPUB state member. An already-ON document skips detection and stays ON on future opens. This setting is not global and needs no `.sav` or permanent sidecar.

Subsequent pages do not detect or shape Arabic in an OFF session. Arabic appearing later intentionally remains unshaped until the reader saves there and reopens on an Arabic page. If the automatic save fails, ON remains active in RAM but is not guaranteed durable; retry with Reader Start. No body text is changed. Switching geometry preserves the byte anchor but rejects incompatible Back history and partial rebuild state.

## Display policy (when shaping is ON)

Logical UTF-8 stays in TXT and normalized EPUB text. Ordinary Arabic letters join; four lam-alef variants are supported. Harakat and the agreed Arabic combining-mark ranges are transparent for display only. Missing meaningful extended Arabic letters use `?`. Arabic comma displays as SuperFW comma and Arabic-Indic digits as SuperFW digits. ZWNJ prevents joining; ZWJ can join neighbors. Persian/Urdu extensions and encoded presentation forms are not full-coverage scripts.

Each already wrapped line uses its first strong character for base direction. Arabic-base lines align to the right body margin; Latin-base lines remain left aligned. Latin and digit islands stay LTR; paired brackets use their enclosed strong direction, and neutrals follow matching neighbors or the base direction. This is the accepted small bidi policy, **not full Unicode UAX #9**: nesting, unmatched brackets, explicit embedding/isolate controls and paragraph-wide direction inheritance are not fully supported. No Arabic typing/editor mode is added.

Latin, Korean/Hangul, CJK, Greek/Cyrillic and other inherited fonts remain unchanged. Arabic glyphs occupy their native 11px artwork within the existing 16px row; no scaling or UI palette changes. Non-Arabic valid UTF-8 body lines call the original renderer unchanged. Malformed body bytes use the bounded decoder instead of forwarding truncated sequences to the legacy painter. Mixed lines use the same original SuperFW glyph renderer for non-Arabic items.

## Layout, persistence and bounds

Reader lines retain the existing 256-byte array (255 text bytes plus NUL). One shared non-reentrant 256-cluster shaping scratch lives in device EWRAM; immutable glyph tables remain ROM-resident. When ON, each bounded Arabic candidate prefix is shaped before acceptance; native advances plus rightmost artwork bounds drive wrapping and drawing. OFF sessions bypass Arabic shaping and the additional per-line rendering scan. Spaces remain word-wrap opportunities, oversized words split across rows, and explicit newline behavior stays unchanged. Per-line work is bounded by the fixed byte cap; no whole-book allocation is added. Long runs of hidden marks can occupy multiple logical rows due to this byte cap.

The existing source cursor advances by original consumed UTF-8 bytes. Neither shaping nor hidden marks rewrite text or byte anchors. The existing checksummed state distinguishes shaping mode and page-layout compatibility. Missing legacy flags default OFF and receive the opening-page check; the previous V1.2 always-shaped layout is not evidence of a saved sticky ON flag. Compatible history and partial rebuilds remain reusable; incompatible geometry keeps the saved byte anchor/settings and rebuilds history. The existing ZIP/cache and FatFS transaction paths are retained; first-time activation now requests an automatic state save.

## Verification commands

```sh
bash tests/run_host_tests.sh
EXTRA_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie' bash tests/run_host_tests.sh
g++ -std=c++17 -Iinclude tests/arabic_reference_probe.cpp -o /tmp/arabic-probe
python tests/test_arabic_reference.py tests/fixtures/arabic/Ghoulam-Regular.ttf /tmp/arabic-probe
python tools/extract_ghoulam.py tests/fixtures/arabic/Ghoulam-Regular.ttf --output /tmp/ghoulam.h
cmp include/ghoulam_data.h /tmp/ghoulam.h
```

The host suites exercise real-font TXT, original DEFLATE EPUB and reopened normalized EPUB cache, multiline long Arabic, marks, mixed Latin/digits/full-width glyphs, all-page forward/back, resume, old/current layout markers, source preservation and bus-safe native pixel bounds. Developer-only TTF/samples/generated fixtures are excluded from ROM DATA and release staging. Emulator language appearance and real Supercard operation remain separate acceptance checks; host pixels are not an emulator claim.

## In-ROM Credits audit

Seven Credits pages are drawn from `src/reader_credits.cpp`. Page 1 contains only the exact personal four-line block; pages 2–7 name Ghoulam (Imad AlFil / mloukhiyye, CC BY 4.0), SuperFW (David Guillen Fandos, GPL v3+), UNSCII (Viznut; full variant GPL, including Fixedsys public-domain glyphs), GNU Unifont/Hangul (Roman Czyborra, Paul Hardy and contributors, GPL v2+ with font exception), Butano (Gustavo Valiente, zlib), devkitARM/devkitPro, FatFs (ChaN, permissive), miniz (MIT; Geldreich/Tenacious/RAD/Valve) and base-project provenance. Left/Right changes Credits pages; B/Start closes. The first page retains inherited personal-text positions/pixels. Tests cover all later-page text/pixel bounds and required notices, plus paging limits, entry release gating and closure.

Audit sources: the read-only gbavocab Credits table; bundled SuperFW font README, renderer and Hangul notices; `include/common_variable_8x16_sprite_font.h`; `src/ff.c`; `third_party/miniz/LICENSE`; and the upstream Ghoulam, UNSCII and GNU Unifont pages. **Inherited provenance limitation:** the exact UNSCII/Unifont snapshot versions are not recorded in this baseline. The conservative GPL attribution is retained; the newer alternative OFL license is not claimed. Framework dependency notices remain in the Butano source distribution. See the full controls PDF for source/license links.
