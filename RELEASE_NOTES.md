# v0.4.1 — Reading layout and controls

- Fix paragraph-aware pagination so the final line stays fully visible.
- `Start` saves the current TXT book; `L` opens settings.
- `R` also advances to the next page.

# GBA Reader v0.4.1 release notes

GBA Reader v0.4.1 substantially improves compatibility with real-world UTF-8 EPUB 2/3 books while keeping parsing bounded for Game Boy Advance hardware.

## EPUB compatibility

- Decodes percent-encoded manifest paths such as `My%20Chapter.xhtml` without treating `+` as a space.
- Decodes XML's predefined entities and numeric character references in `full-path`, `href`, `id`, `idref`, and media-type attributes.
- Prefers the standard EPUB package rootfile when a container declares multiple renditions.
- Supports CDATA visible text and adds readable breaks for common semantic XHTML and table elements.
- Accepts case-insensitive XHTML media types.
- Raises the readable spine limit from 64 to 256 documents and the internal archive-path limit from 191 to 255 bytes.
- Stores compact central-directory offsets for spine items instead of full path and ZIP metadata copies, reducing retained spine-index memory while increasing capacity.

## ZIP correctness and safety

- Revalidates local headers, extra fields, redundant CRC/size fields, and data descriptors for every required metadata and spine entry, including entries whose filenames look like image assets.
- Rejects required payloads that overlap the central directory.
- Rejects duplicate required ZIP member names instead of selecting an order-dependent copy.
- Rejects malformed URI escapes, encoded NULs, encoded separators, and traversal introduced through percent decoding.

## Validation

- Expanded host regression coverage to 61 EPUB test cases across 60 distinct generated fixture files.
- Tested the parser against five Project Gutenberg EPUBs and 45 packaged EPUB 3 sample books. Of those EPUB 3 samples, 40 open successfully: 39 yield readable text and one valid empty-text sample yields no output. The remaining five are intentionally outside the reader's scope (four image/SVG-only spines and one chapter above the documented 4 MiB limit).

## Scope retained

- EPUB package metadata and XHTML must be UTF-8.
- Images, SVG presentation, CSS presentation, JavaScript, fonts, media, DRM, ZIP64, and Arabic shaping/bidirectional rendering remain unsupported.
- Text size remains fixed at the native 16-pixel SuperFW size.
