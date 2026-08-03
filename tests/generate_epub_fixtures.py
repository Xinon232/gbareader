#!/usr/bin/env python3
import sys
import zipfile
import struct
from pathlib import Path

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)

container = '''<?xml version="1.0"?><container><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'''

def add(z, name, data):
    info = zipfile.ZipInfo(name, (2020, 1, 1, 0, 0, 0))
    info.compress_type = z.compression
    info.external_attr = 0o100644 << 16
    z.writestr(info, data)

def make(name, compression, text):
    opf = '''<package><manifest><item id="c" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="c"/></spine></package>'''
    with zipfile.ZipFile(out / name, "w", compression=compression) as z:
        add(z, "META-INF/container.xml", container)
        add(z, "OEBPS/book.opf", opf)
        add(z, "OEBPS/chapter.xhtml", f"<html><body><p>{text}</p></body></html>")

make("stored.epub", zipfile.ZIP_STORED, "Stored chapter.")
make("deflated.epub", zipfile.ZIP_DEFLATED, "Deflated chapter.")

def custom(name, opf, chapters, compression=zipfile.ZIP_STORED, container_xml=container):
    with zipfile.ZipFile(out / name, "w", compression=compression) as z:
        if container_xml is not None:
            add(z, "META-INF/container.xml", container_xml)
        if opf is not None:
            add(z, "OEBPS/book.opf", opf)
        for path, body in chapters:
            add(z, path, body)

ordered_opf = '''<package><manifest>
<item id="one" href="one.xhtml"/><item id="two" href="two.xhtml"/>
</manifest><spine><itemref idref="two"/><itemref idref="one"/></spine></package>'''
custom("ordered.epub", ordered_opf, [
    ("OEBPS/one.xhtml", "<html><body><p>First file.</p></body></html>"),
    ("OEBPS/two.xhtml", "<html><head><style>hidden</style></head><body><h1>Second</h1><p>A &amp; &lt; &gt; &quot; &apos; &nbsp; &#65; &#x41; &bogus;</p><ul><li>Item</li></ul><script>hidden</script></body></html>"),
])

custom("missing-container.epub", ordered_opf, [], container_xml=None)
custom("missing-rootfile.epub", ordered_opf, [], container_xml="<container/>")
custom("missing-manifest.epub", '<package><manifest></manifest><spine><itemref idref="nope"/></spine></package>', [])
custom("missing-spine.epub", '<package><manifest><item id="one" href="one.xhtml"/></manifest><spine/></package>', [("OEBPS/one.xhtml", "<p>x</p>")])
custom("traversal.epub", '<package><manifest><item id="x" href="../../../evil.xhtml"/></manifest><spine><itemref idref="x"/></spine></package>', [("evil.xhtml", "<p>x</p>")])
custom("declared-large.epub", ordered_opf, [("OEBPS/one.xhtml", "<p>x</p>"), ("OEBPS/two.xhtml", "<p>y</p>")])
custom("extracted-large.epub", '<package><manifest><item id="x" href="big.xhtml"/></manifest><spine><itemref idref="x"/></spine></package>', [("OEBPS/big.xhtml", "<p>" + "x" * 65530 + "</p>")], zipfile.ZIP_DEFLATED)

prefixed_container = '''<?xml version="1.0"?><c:container><c:rootfiles><c:rootfile\n full-path="OEBPS/book.opf" /></c:rootfiles></c:container>'''
prefixed_opf = '''<opf:package><opf:manifest><opf:item\n href="chapter.xhtml" media-type="application/xhtml+xml" id="c" /></opf:manifest><opf:spine><opf:itemref\n idref="c" /></opf:spine></opf:package>'''
custom("prefixed-whitespace.epub", prefixed_opf, [("OEBPS/chapter.xhtml", "<p>Namespaced chapter.</p>")], container_xml=prefixed_container)
custom("wrong-media.epub", '<package><manifest><item id="c" href="chapter.xhtml" media-type="image/svg+xml"/></manifest><spine><itemref idref="c"/></spine></package>', [("OEBPS/chapter.xhtml", "<p>Not text.</p>")])
custom("self-closing-suppressed.epub", '<package><manifest><item id="c" href="chapter.xhtml"/></manifest><spine><itemref idref="c"/></spine></package>', [("OEBPS/chapter.xhtml", "<html><head/><style/><script/><body><p>Still visible.</p></body></html>")])
custom("entities.epub", '<package><manifest><item id="c" href="chapter.xhtml"/></manifest><spine><itemref idref="c"/></spine></package>', [("OEBPS/chapter.xhtml", "<p>&mdash;&ndash;&hellip;&copy;&lsquo;&rsquo;&ldquo;&rdquo;&reg;&trade;</p>")])
custom("ignored-asset.epub", '<package><manifest><item id="c" href="chapter.xhtml" media-type="application/xhtml+xml"/><item id="cover" href="cover.bin" media-type="application/octet-stream"/></manifest><spine><itemref idref="c"/></spine></package>', [("OEBPS/chapter.xhtml", "<p>Readable chapter.</p>"), ("OEBPS/cover.bin", bytes(range(256)) * 300)])

many_opf = '<package><manifest><item id="c" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="c"/></spine></package>'
with zipfile.ZipFile(out / "many-entries.epub", "w", compression=zipfile.ZIP_DEFLATED) as z:
    for index in range(150):
        add(z, f"OEBPS/assets/prefix-{index:03d}.bin", bytes((index & 0xFF,)))
    add(z, "META-INF/container.xml", container)
    add(z, "OEBPS/book.opf", many_opf)
    add(z, "OEBPS/chapter.xhtml", "<html><body><p>Many assets, readable text.</p></body></html>")
    for index in range(150):
        add(z, f"OEBPS/assets/suffix-{index:03d}.bin", bytes((index & 0xFF,)))

scoped_opf = '''<package>
<itemref idref="outside"/><item id="c" href="wrong.xhtml"/>
<manifest><!-- </manifest><item id="c" href="comment.xhtml"/> --><item data-id="wrong" id="c" href="right.xhtml"/><item id="two" href="two.xhtml"/></manifest>
<spine><!-- <itemref idref="outside"/> --><itemref idref="two"/><itemref idref="c"/></spine>
</package>'''
custom("scoped-opf.epub", scoped_opf, [
    ("OEBPS/wrong.xhtml", "<p>Wrong global match.</p>"),
    ("OEBPS/right.xhtml", "<p>Right manifest.</p>"),
    ("OEBPS/two.xhtml", "<p>Declared first.</p>"),
])
quoted_container = '''<container><rootfiles><rootfile note="1 > 0" full-path="OEBPS/book.opf"/></rootfiles></container>'''
quoted_opf = '''<package><manifest note='2 > 1'><item title="3 > 2" id="c" href="chapter.xhtml"/></manifest><spine title='4 > 3'><itemref note="5 > 4" idref="c"/></spine></package>'''
custom("quoted-tags.epub", quoted_opf, [("OEBPS/chapter.xhtml", "<body xmlns:x='urn:test'><x:p title=\"1 > 0\">Visible</x:p><x:p title='2 > 1'>single</x:p></body>")], container_xml=quoted_container)
custom("unterminated-opf-tag.epub", '<package><manifest><item title="unterminated > <spine><itemref idref="c"/></spine></package>', [("OEBPS/chapter.xhtml", "<p>x</p>")])
custom("unterminated-xhtml-tag.epub", '<package><manifest><item id="c" href="chapter.xhtml"/></manifest><spine><itemref idref="c"/></spine></package>', [("OEBPS/chapter.xhtml", '<body><span title="unterminated >Visible</span></body>')])

def patch_methods(src, dst, method, target=None):
    data = bytearray((out / src).read_bytes())
    pos = 0
    while True:
        pos = data.find(b"PK\x03\x04", pos)
        if pos < 0: break
        name_len = struct.unpack_from("<H", data, pos + 26)[0]
        name = data[pos + 30:pos + 30 + name_len]
        if target is None or name.endswith(target): struct.pack_into("<H", data, pos + 8, method)
        pos += 4
    pos = 0
    while True:
        pos = data.find(b"PK\x01\x02", pos)
        if pos < 0: break
        name_len = struct.unpack_from("<H", data, pos + 28)[0]
        name = data[pos + 46:pos + 46 + name_len]
        if target is None or name.endswith(target): struct.pack_into("<H", data, pos + 10, method)
        pos += 4
    (out / dst).write_bytes(data)

patch_methods("stored.epub", "unsupported.epub", 12, b"chapter.xhtml")
patch_methods("ignored-asset.epub", "ignored-asset.epub", 12, b"cover.bin")

encrypted = bytearray((out / "stored.epub").read_bytes())
for sig, field in ((b"PK\x03\x04", 6), (b"PK\x01\x02", 8)):
    pos = 0
    while True:
        pos = encrypted.find(sig, pos)
        if pos < 0: break
        struct.pack_into("<H", encrypted, pos + field, struct.unpack_from("<H", encrypted, pos + field)[0] | 1); pos += 4
(out / "encrypted.epub").write_bytes(encrypted)

def central_record(data, target):
    pos = 0
    while True:
        pos = data.find(b"PK\x01\x02", pos)
        assert pos >= 0, target
        name_len, extra_len, comment_len = struct.unpack_from("<HHH", data, pos + 28)
        name = bytes(data[pos + 46:pos + 46 + name_len])
        if name == target:
            return pos
        pos += 46 + name_len + extra_len + comment_len

def local_record(data, central):
    pos = struct.unpack_from("<I", data, central + 42)[0]
    assert data[pos:pos + 4] == b"PK\x03\x04"
    return pos

def add_central_extra(src, dst, target, extra):
    data = bytearray((out / src).read_bytes())
    cpos = central_record(data, target)
    name_len, extra_len = struct.unpack_from("<HH", data, cpos + 28)
    insert_at = cpos + 46 + name_len + extra_len
    data[insert_at:insert_at] = extra
    struct.pack_into("<H", data, cpos + 30, extra_len + len(extra))
    eocd = data.rfind(b"PK\x05\x06")
    cd_size = struct.unpack_from("<I", data, eocd + 12)[0]
    struct.pack_into("<I", data, eocd + 12, cd_size + len(extra))
    (out / dst).write_bytes(data)

def add_last_local_extra(src, dst, target, extra):
    data = bytearray((out / src).read_bytes())
    cpos = central_record(data, target)
    lpos = local_record(data, cpos)
    name_len, extra_len = struct.unpack_from("<HH", data, lpos + 26)
    insert_at = lpos + 30 + name_len + extra_len
    data[insert_at:insert_at] = extra
    struct.pack_into("<H", data, lpos + 28, extra_len + len(extra))
    eocd = data.rfind(b"PK\x05\x06")
    cd_offset = struct.unpack_from("<I", data, eocd + 16)[0]
    assert insert_at <= cd_offset
    struct.pack_into("<I", data, eocd + 16, cd_offset + len(extra))
    (out / dst).write_bytes(data)

zip64_extra = struct.pack("<HHQ", 0x0001, 8, 0x123456789ABCDEF0)
add_central_extra("stored.epub", "zip64-central-extra.epub", b"OEBPS/chapter.xhtml", zip64_extra)
add_last_local_extra("stored.epub", "zip64-local-extra.epub", b"OEBPS/chapter.xhtml", zip64_extra)

for fixture, where in (("zip64-central-extra.epub", "central"), ("zip64-local-extra.epub", "local")):
    check = bytearray((out / fixture).read_bytes())
    cp = central_record(check, b"OEBPS/chapter.xhtml")
    lp = local_record(check, cp)
    compressed_size, uncompressed_size = struct.unpack_from("<II", check, cp + 20)
    local_offset = struct.unpack_from("<I", check, cp + 42)[0]
    assert compressed_size != 0xFFFFFFFF
    assert uncompressed_size != 0xFFFFFFFF
    assert local_offset != 0xFFFFFFFF
    pos = cp + 46 + struct.unpack_from("<H", check, cp + 28)[0] if where == "central" else lp + 30 + struct.unpack_from("<H", check, lp + 26)[0]
    assert struct.unpack_from("<H", check, pos)[0] == 0x0001

add_central_extra("stored.epub", "malformed-central-extra.epub", b"OEBPS/chapter.xhtml", struct.pack("<HH", 0xCAFE, 5) + b"x")
add_last_local_extra("stored.epub", "malformed-local-extra.epub", b"OEBPS/chapter.xhtml", struct.pack("<HH", 0xCAFE, 5) + b"x")

locator = bytearray((out / "stored.epub").read_bytes())
eocd = locator.rfind(b"PK\x05\x06")
locator[eocd:eocd] = struct.pack("<IIQI", 0x07064B50, 0, 0, 1)
(out / "zip64-locator.epub").write_bytes(locator)

crc_bad = bytearray((out / "stored.epub").read_bytes())
cpos = central_record(crc_bad, b"OEBPS/chapter.xhtml")
old_crc = struct.unpack_from("<I", crc_bad, cpos + 16)[0]
struct.pack_into("<I", crc_bad, cpos + 16, old_crc ^ 0x80000000)
assert struct.unpack_from("<I", crc_bad, cpos + 16)[0] != old_crc
(out / "crc-central-mismatch.epub").write_bytes(crc_bad)

payload_bad = bytearray((out / "stored.epub").read_bytes())
cpos = central_record(payload_bad, b"OEBPS/chapter.xhtml")
lpos = local_record(payload_bad, cpos)
name_len, extra_len = struct.unpack_from("<HH", payload_bad, lpos + 26)
payload = lpos + 30 + name_len + extra_len
payload_bad[payload + 15] ^= 1
assert payload_bad[payload + 15] != (out / "stored.epub").read_bytes()[payload + 15]
(out / "corrupt-payload.epub").write_bytes(payload_bad)

name_bad = bytearray((out / "stored.epub").read_bytes())
cpos = central_record(name_bad, b"OEBPS/chapter.xhtml")
lpos = local_record(name_bad, cpos)
name_len = struct.unpack_from("<H", name_bad, lpos + 26)[0]
local_name = lpos + 30
assert bytes(name_bad[local_name:local_name + name_len]) == b"OEBPS/chapter.xhtml"
name_bad[local_name + name_len - 1] = ord("y")
assert bytes(name_bad[local_name:local_name + name_len]) != b"OEBPS/chapter.xhtml"
(out / "local-name-mismatch.epub").write_bytes(name_bad)

flags_bad = bytearray((out / "stored.epub").read_bytes())
cpos = central_record(flags_bad, b"OEBPS/chapter.xhtml")
lpos = local_record(flags_bad, cpos)
central_flags = struct.unpack_from("<H", flags_bad, cpos + 8)[0]
local_flags = struct.unpack_from("<H", flags_bad, lpos + 6)[0]
assert central_flags == local_flags
struct.pack_into("<H", flags_bad, lpos + 6, local_flags ^ 0x0008)
assert struct.unpack_from("<H", flags_bad, lpos + 6)[0] != central_flags
(out / "local-flags-mismatch.epub").write_bytes(flags_bad)

def patch_local_u32(src, dst, target, field, transform):
    data = bytearray((out / src).read_bytes())
    cpos = central_record(data, target)
    lpos = local_record(data, cpos)
    central_field = {14: 16, 18: 20, 22: 24}[field]
    central_value = struct.unpack_from("<I", data, cpos + central_field)[0]
    local_value = struct.unpack_from("<I", data, lpos + field)[0]
    assert local_value == central_value
    struct.pack_into("<I", data, lpos + field, transform(local_value))
    assert struct.unpack_from("<I", data, lpos + field)[0] != central_value
    (out / dst).write_bytes(data)

patch_local_u32("stored.epub", "local-crc-mismatch.epub", b"OEBPS/chapter.xhtml", 14, lambda value: value ^ 0x80000000)
patch_local_u32("stored.epub", "local-compressed-size-mismatch.epub", b"OEBPS/chapter.xhtml", 18, lambda value: value + 1)
patch_local_u32("stored.epub", "local-uncompressed-size-mismatch.epub", b"OEBPS/chapter.xhtml", 22, lambda value: value + 1)

def make_descriptor(dst, descriptor_kind):
    data = bytearray((out / "stored.epub").read_bytes())
    target = b"OEBPS/chapter.xhtml"
    cpos = central_record(data, target)
    lpos = local_record(data, cpos)
    crc, compressed_size, uncompressed_size = struct.unpack_from("<III", data, cpos + 16)
    name_len, extra_len = struct.unpack_from("<HH", data, lpos + 26)
    data_end = lpos + 30 + name_len + extra_len + compressed_size
    eocd = data.rfind(b"PK\x05\x06")
    original_cd_offset = struct.unpack_from("<I", data, eocd + 16)[0]
    assert data_end == original_cd_offset
    local_flags = struct.unpack_from("<H", data, lpos + 6)[0] | 0x0008
    central_flags = struct.unpack_from("<H", data, cpos + 8)[0] | 0x0008
    struct.pack_into("<H", data, lpos + 6, local_flags)
    struct.pack_into("<III", data, lpos + 14, 0, 0, 0)

    if descriptor_kind == "signed":
        descriptor = struct.pack("<IIII", 0x08074B50, crc, compressed_size, uncompressed_size)
    elif descriptor_kind == "unsigned":
        descriptor = struct.pack("<III", crc, compressed_size, uncompressed_size)
    elif descriptor_kind == "absent":
        descriptor = b""
    elif descriptor_kind == "bad-crc":
        descriptor = struct.pack("<IIII", 0x08074B50, crc ^ 0x80000000, compressed_size, uncompressed_size)
    elif descriptor_kind == "bad-compressed-size":
        descriptor = struct.pack("<IIII", 0x08074B50, crc, compressed_size + 1, uncompressed_size)
    elif descriptor_kind == "truncated":
        descriptor = struct.pack("<II", 0x08074B50, crc)
    else:
        raise AssertionError(descriptor_kind)

    data[data_end:data_end] = descriptor
    cpos += len(descriptor)
    assert data[cpos:cpos + 4] == b"PK\x01\x02"
    struct.pack_into("<H", data, cpos + 8, central_flags)
    eocd = data.rfind(b"PK\x05\x06")
    cd_offset = struct.unpack_from("<I", data, eocd + 16)[0]
    struct.pack_into("<I", data, eocd + 16, cd_offset + len(descriptor))

    # Fixture self-check: ordinary central values, zero local placeholders, and
    # descriptor bytes beginning at the exact central-size-derived data end.
    assert struct.unpack_from("<H", data, lpos + 6)[0] & 0x0008
    assert struct.unpack_from("<H", data, cpos + 8)[0] & 0x0008
    assert struct.unpack_from("<III", data, lpos + 14) == (0, 0, 0)
    assert struct.unpack_from("<III", data, cpos + 16) == (crc, compressed_size, uncompressed_size)
    assert compressed_size != 0xFFFFFFFF and uncompressed_size != 0xFFFFFFFF
    assert data_end == lpos + 30 + name_len + extra_len + struct.unpack_from("<I", data, cpos + 20)[0]
    assert bytes(data[data_end:data_end + len(descriptor)]) == descriptor
    (out / dst).write_bytes(data)

make_descriptor("descriptor-signed.epub", "signed")
make_descriptor("descriptor-unsigned.epub", "unsigned")
make_descriptor("descriptor-absent.epub", "absent")
make_descriptor("descriptor-bad-crc.epub", "bad-crc")
make_descriptor("descriptor-bad-compressed-size.epub", "bad-compressed-size")
make_descriptor("descriptor-truncated.epub", "truncated")

trailing = bytearray((out / "deflated.epub").read_bytes())
cpos = central_record(trailing, b"OEBPS/chapter.xhtml")
lpos = local_record(trailing, cpos)
compressed_size = struct.unpack_from("<I", trailing, cpos + 20)[0]
name_len, extra_len = struct.unpack_from("<HH", trailing, lpos + 26)
insert_at = lpos + 30 + name_len + extra_len + compressed_size
trailing[insert_at:insert_at] = b"\x00"
# Insertion shifts this central record and EOCD by one byte.
cpos += 1
assert trailing[cpos:cpos + 4] == b"PK\x01\x02"
struct.pack_into("<I", trailing, cpos + 20, compressed_size + 1)
eocd = trailing.rfind(b"PK\x05\x06")
old_cd_offset = struct.unpack_from("<I", trailing, eocd + 16)[0]
struct.pack_into("<I", trailing, eocd + 16, old_cd_offset + 1)
assert struct.unpack_from("<I", trailing, cpos + 20)[0] == compressed_size + 1
(out / "deflate-trailing.epub").write_bytes(trailing)

commented = bytearray((out / "stored.epub").read_bytes())
eocd = commented.rfind(b"PK\x05\x06")
assert eocd + 22 == len(commented)
comment = b"comment-prefix-PK\x05\x06" + b"\x00" * 18 + b"-suffix"
struct.pack_into("<H", commented, eocd + 20, len(comment))
commented.extend(comment)
assert eocd + 22 + struct.unpack_from("<H", commented, eocd + 20)[0] == len(commented)
assert commented.rfind(b"PK\x05\x06") > eocd
(out / "fake-eocd-comment.epub").write_bytes(commented)

declared = bytearray((out / "declared-large.epub").read_bytes())
pos = declared.find(b"PK\x01\x02")
while pos >= 0:
    name_len = struct.unpack_from("<H", declared, pos + 28)[0]
    name = declared[pos + 46:pos + 46 + name_len]
    if name.endswith(b"one.xhtml"):
        lpos = local_record(declared, pos)
        struct.pack_into("<I", declared, pos + 24, 65537)
        struct.pack_into("<I", declared, lpos + 22, 65537)
    pos = declared.find(b"PK\x01\x02", pos + 4)
(out / "declared-large.epub").write_bytes(declared)

extracted = bytearray((out / "extracted-large.epub").read_bytes())
pos = extracted.find(b"PK\x01\x02")
while pos >= 0:
    name_len = struct.unpack_from("<H", extracted, pos + 28)[0]
    name = extracted[pos + 46:pos + 46 + name_len]
    if name.endswith(b"big.xhtml"):
        lpos = local_record(extracted, pos)
        struct.pack_into("<I", extracted, pos + 24, 32)
        struct.pack_into("<I", extracted, lpos + 22, 32)
    pos = extracted.find(b"PK\x01\x02", pos + 4)
(out / "extracted-large.epub").write_bytes(extracted)

raw = (out / "stored.epub").read_bytes()
(out / "truncated.epub").write_bytes(raw[:-9])
