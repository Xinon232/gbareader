# GBA Reader v0.2.0 release notes

GBA Reader v0.2.0 adds direct, read-only, text-only EPUB reading from the
Supercard SD root while preserving v0.1.0 TXT reading, pagination, controls,
font/settings and SRAM byte-offset resume.

Supported EPUBs are ZIP files using stored or raw-DEFLATE members with CRC-32
integrity checks for required metadata and spine members, and a
`META-INF/container.xml`, an OPF manifest/spine, and bounded XHTML/HTML spine
documents. Reading order follows the OPF spine. Common block elements and
entities are converted to visible UTF-8 text.

This release does not claim full EPUB compliance. ZIP64, multi-disk archives,
encryption/DRM, other compression methods, Arabic, images, CSS presentation,
JavaScript, embedded fonts, audio, video and SVG are unsupported. See README
for the compile-time limits.
