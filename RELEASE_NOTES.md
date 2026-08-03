# GBA Reader v0.2.2 release notes

GBA Reader v0.2.2 improves compatibility and opening speed for image-heavy,
text-readable EPUB books.

## Fixed

- Removed the old 128-ZIP-entry rejection that caused valid EPUBs with many
  images and other assets to fail with “EPUB has too many files”.
- The ZIP central directory is now validated as a bounded stream. Only the
  container, package document and readable spine entries are retained for use.
- Required EPUB files can occur anywhere in central-directory order.

## Improved image handling

- Common image archive members are recognized case-insensitively by extension
  and skipped before their local headers or compressed payloads are processed.
- OPF spine entries declared as `image/*` are omitted from the text stream, so
  an image-only cover or page no longer blocks later readable chapters.
- Supported skipped extensions include JPG/JPEG, PNG, GIF, WebP, SVG/SVGZ,
  BMP, AVIF, TIFF, ICO, JXL, HEIC and HEIF.

## Preserved behavior

- TXT and direct text-only EPUB reading remains read-only.
- Per-book filename-based resume, automatic page saves and manual `R` saves
  from v0.2.1 remain available.
- Line spacing and top/bottom margins remain adjustable from 1 through 4.
- The fixed native 16-pixel SuperFW body font remains unchanged.
- Arabic, CSS presentation, JavaScript, embedded fonts, audio, video, DRM,
  ZIP64 and fixed-layout rendering remain unsupported.

The release archive contains only `gbareader.gba`.
