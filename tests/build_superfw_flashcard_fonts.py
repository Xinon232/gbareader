#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
import struct

ROOT = Path(__file__).resolve().parents[1]
SUP = ROOT / 'references/superfw/res'
PACK = (SUP / 'fonts.pack').read_bytes()
PACK_EXT = (SUP / 'fonts-ext.pack').read_bytes()
ASCII_BMP = Image.open(ROOT / 'graphics/vocab_latin_old_ext_font.bmp').convert('P')
PALETTE = ASCII_BMP.getpalette()
CELL_H = 16


def blocks(pack):
    count = pack[3]
    base = 8 + count * 16
    out = []
    for i in range(count):
        start, end, flags, boff = struct.unpack_from('<IIII', pack, 8 + i * 16)
        out.append((start, end, flags, base + boff, base, boff))
    return out


def find_block(pack, start, end):
    for bs, be, flags, abs_off, base, boff in blocks(pack):
        if bs == start and be == end:
            return bs, be, flags, abs_off
    raise RuntimeError(f'block U+{start:04X}-U+{end:04X} not found')


def utf8_char(cp):
    return chr(cp)


def esc_cpp_char(ch):
    if ch == '\\': return '\\\\'
    if ch == '"': return '\\"'
    return ch


def draw_var_glyph(pack, bs, be, idx_off, cp, cell_w):
    n = be - bs + 1
    chdata_off = idx_off + n * 2
    ent = struct.unpack_from('<H', pack, idx_off + (cp - bs) * 2)[0]
    w = (ent >> 13) + 1
    off = ent & 0x1fff
    glyph = Image.new('P', (cell_w, CELL_H), 0)
    glyph.putpalette(PALETTE)
    pix = [0] * (cell_w * CELL_H)
    for x in range(min(w, cell_w)):
        col = struct.unpack_from('<H', pack, chdata_off + (off + x) * 2)[0]
        for y in range(CELL_H):
            if col & (1 << y):
                pix[y * cell_w + x] = 1
    glyph.putdata(pix)
    return glyph, min(w + 1 if w < cell_w else cell_w, cell_w)


def draw_fixed_glyph(pack, bs, be, idx_off, cp, cell_w):
    off = idx_off + (cp - bs) * CELL_H * 2
    glyph = Image.new('P', (cell_w, CELL_H), 0)
    glyph.putpalette(PALETTE)
    pix = [0] * (cell_w * CELL_H)
    for x in range(min(16, cell_w)):
        col = struct.unpack_from('<H', pack, off + x * 2)[0]
        for y in range(CELL_H):
            if col & (1 << y):
                pix[y * cell_w + x] = 1
    glyph.putdata(pix)
    return glyph, cell_w


def make_font(name, cell_w, ranges, namespace=None):
    # Rows 0..93 are Butano's printable ASCII glyph graphics ('!' through '~').
    # Space has a width entry but no graphic row, matching the existing Butano
    # sprite-font assets in this project.
    cps = []
    for start, end, _pack_name in ranges:
        cps.extend(range(start, end + 1))
    rows = 94 + len(cps)
    out = Image.new('P', (cell_w, CELL_H * rows), 0)
    out.putpalette(PALETTE)

    widths = []
    # Copy ASCII from existing flashcard font. For 16px-wide East Asian fonts,
    # keep ASCII visually identical and leave right side blank.
    for row in range(94):
        src = ASCII_BMP.crop((0, row * CELL_H, 8, (row + 1) * CELL_H))
        out.paste(src, (0, row * CELL_H))
        widths.append(8)
    widths.insert(0, 8)  # width for space

    for row, cp in enumerate(cps, start=94):
        # Pick the declared source pack and locate the covering block.
        pack_name = next(p for s, e, p in ranges if s <= cp <= e)
        pack = PACK_EXT if pack_name == 'ext' else PACK
        cache_key = (pack_name, cp)
        blk = None
        for bs, be, flags, abs_off, base, boff in blocks(pack):
            if bs <= cp <= be:
                blk = (bs, be, flags, abs_off)
                break
        if blk is None:
            raise RuntimeError(f'U+{cp:04X} missing from {pack_name}')
        bs, be, flags, abs_off = blk
        if flags & 0x80000000:
            raise RuntimeError(f'U+{cp:04X} is compositional; use fonts-ext direct pack')
        if flags & 0x1:
            glyph, width = draw_fixed_glyph(pack, bs, be, abs_off, cp, cell_w)
        else:
            glyph, width = draw_var_glyph(pack, bs, be, abs_off, cp, cell_w)
        out.paste(glyph, (0, row * CELL_H))
        widths.append(width)

    bmp_path = ROOT / f'graphics/{name}.bmp'
    json_path = ROOT / f'graphics/{name}.json'
    out.save(bmp_path)
    json_path.write_text('{\n    "type": "sprite",\n    "height": 16\n}\n')

    char_list = ', '.join(f'"{esc_cpp_char(utf8_char(cp))}"' for cp in cps)
    widths_s = ', '.join(str(int(w)) for w in widths)
    guard = name.upper() + '_SPRITE_FONT_H'
    var = name + '_sprite_font'
    ns_open = f'namespace {namespace}\n{{\n\n' if namespace else ''
    ns_close = f'\n}}\n' if namespace else ''
    header = f'''#ifndef {guard}
#define {guard}

#include "bn_sprite_font.h"
#include "bn_utf8_characters_map.h"
#include "bn_sprite_items_{name}.h"

{ns_open}constexpr bn::utf8_character {var}_utf8_characters[] = {{
    {char_list}
}};

constexpr bn::span<const bn::utf8_character> {var}_utf8_characters_span(
        {var}_utf8_characters);

constexpr auto {var}_utf8_characters_map =
        bn::utf8_characters_map<{var}_utf8_characters_span>();

constexpr int8_t {var}_character_widths[] = {{
    {widths_s}
}};

constexpr bn::sprite_font {var}(
        bn::sprite_items::{name},
        {var}_utf8_characters_map.reference(),
        {var}_character_widths);
{ns_close}
#endif
'''
    (ROOT / f'include/{name}_sprite_font.h').write_text(header, encoding='utf-8')
    print(f'wrote {name}: {out.size}, extra chars={len(cps)}, width={cell_w}, bytes={bmp_path.stat().st_size}')


def main():
    make_font('vocab_superfw_latin_ext_font', 8, [
        (0x0080, 0x024F, 'main'),
    ], namespace='vocab_font')
    make_font('vocab_superfw_greek_cyrillic_font', 8, [
        (0x0370, 0x04FF, 'main'),
    ], namespace='vocab_font')
    make_font('vocab_superfw_japanese_font', 16, [
        (0x3000, 0x3009, 'main'),
        (0x3040, 0x309F, 'main'),
        (0x30A0, 0x30FF, 'main'),
    ], namespace='vocab_font')
    make_font('vocab_superfw_cjk_font', 16, [
        (0x4E00, 0x9FEF, 'main'),
        (0x20000, 0x200CC, 'main'),
    ], namespace='vocab_font')
    make_font('vocab_superfw_hangul_font', 16, [
        (0xAC00, 0xD7A3, 'ext'),
    ], namespace='vocab_font')

if __name__ == '__main__':
    main()
