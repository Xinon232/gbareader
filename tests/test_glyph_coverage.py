#!/usr/bin/env python3
import html
import struct
import sys
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
fixtures = Path(sys.argv[1])


def load_pack(path):
    data = path.read_bytes()
    assert data[:2] == b"FO" and data[2] == 1
    block_count = data[3]
    assert struct.unpack_from("<I", data, 4)[0] == len(data)
    base = 8 + block_count * 16
    supported = set()
    for index in range(block_count):
        start, end, flags, offset = struct.unpack_from("<IIII", data, 8 + index * 16)
        block = base + offset
        if flags & 1:
            supported.update(range(start, end + 1))
        else:
            for codepoint in range(start, end + 1):
                entry = struct.unpack_from("<H", data, block + (codepoint - start) * 2)[0]
                if entry != 0xFFFF:
                    supported.add(codepoint)
    return supported


supported = set(range(0x80))
supported |= load_pack(root / "references/superfw/res/fonts.pack")
supported |= load_pack(root / "references/superfw/res/reader-symbols.pack")

required = {
    0x0022,  # straight double quote
    0x00A3,  # pound
    0x00A5,  # yen
    0x2013, 0x2014,  # dashes
    0x2018, 0x2019, 0x201C, 0x201D,  # smart quotes
    0x2022, 0x2026,  # bullet and ellipsis
    0x20AC,  # euro
    0x2122,  # trademark
}
missing_required = sorted(required - supported)
assert not missing_required, "missing required glyphs: " + ", ".join(f"U+{cp:04X}" for cp in missing_required)
assert 0x1F642 not in supported  # deliberate unsupported fallback case

text = (fixtures / "glyph-corpus.txt").read_text(encoding="utf-8")
with zipfile.ZipFile(fixtures / "glyph-corpus.epub") as archive:
    markup = archive.read("OEBPS/chapter.xhtml").decode("utf-8")
    epub_text = html.unescape(markup[markup.index("<p>") + 3:markup.index("</p>")])

for label, corpus in (("TXT", text), ("EPUB", epub_text)):
    corpus_missing = sorted({ord(char) for char in corpus if not char.isspace()} - supported)
    assert corpus_missing == [0x1F642], (
        f"{label} unexpected missing glyphs: " +
        ", ".join(f"U+{cp:04X}" for cp in corpus_missing)
    )

print("PASS: synthetic TXT/EPUB glyph coverage")
