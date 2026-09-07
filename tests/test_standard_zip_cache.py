#!/usr/bin/env python3
"""Integration fixture: a cache is an ordinary stored ZIP member, never EOCD tail bytes."""
import sys
import zipfile
from pathlib import Path

root = Path(sys.argv[1])
path = root / "standard-cache-member.epub"
with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as z:
    z.writestr("mimetype", b"application/epub+zip")
    z.writestr("META-INF/container.xml", b"<container/>")
    z.writestr("OEBPS/keep.xhtml", b"<p>original</p>")
    z.writestr("META-INF/gbareader/cache-v5", b"GBAREPC5" + b"\0" * 24 + b"normalized\n")
with zipfile.ZipFile(path) as z:
    assert z.testzip() is None
    assert z.getinfo("META-INF/gbareader/cache-v5").compress_type == zipfile.ZIP_STORED
    assert z.read("mimetype") == b"application/epub+zip"
    assert z.read("OEBPS/keep.xhtml") == b"<p>original</p>"
raw = path.read_bytes()
eocd = raw.rfind(b"PK\x05\x06")
assert eocd >= 0 and eocd + 22 + int.from_bytes(raw[eocd + 20:eocd + 22], "little") == len(raw)

# The actual C++ cache writer must also produce an archive ordinary ZIP readers accept.
cached = Path(sys.argv[2])
original = Path(sys.argv[3])
with zipfile.ZipFile(cached) as z:
    assert z.testzip() is None
    cache = z.getinfo("META-INF/gbareader/cache-v5")
    state = z.getinfo("META-INF/gbareader/state-v5")
    assert cache.compress_type == state.compress_type == zipfile.ZIP_STORED
    assert z.read("OEBPS/two.xhtml") == zipfile.ZipFile(original).read("OEBPS/two.xhtml")
assert cached.read_bytes().endswith(b"PK\x05\x06" + cached.read_bytes()[-18:])
actual = cached.read_bytes()
actual_eocd = actual.rfind(b"PK\x05\x06")
assert actual_eocd + 22 + int.from_bytes(actual[actual_eocd + 20:actual_eocd + 22], "little") == len(actual)
print("PASS: standard ZIP cache-member fixture")
