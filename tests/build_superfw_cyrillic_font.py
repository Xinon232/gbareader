#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
import struct
import re

ROOT = Path(__file__).resolve().parents[1]
pack = (ROOT / 'references/superfw/res/fonts.pack').read_bytes()
count = pack[3]
data_base = 8 + count * 16
cyr_block = None
for i in range(count):
    start, end, flags, boff = struct.unpack_from('<IIII', pack, 8 + i * 16)
    if start <= 0x0400 and end >= 0x04ff and flags == 0:
        cyr_block = (start, end, boff)
        break
assert cyr_block, 'No SuperFW Cyrillic block found'
start, end, boff = cyr_block
idx_off = data_base + boff
n = end - start + 1
chdata_off = idx_off + n * 2

# ASCII rows from the current old Latin/SuperFW-style flashcard font.
latin_bmp = Image.open(ROOT / 'graphics/vocab_latin_old_ext_font.bmp').convert('P')
cell_w = 8
cell_h = 16
chars = [chr(cp) for cp in range(0x0400, 0x0500)]
rows = 94 + len(chars)
out = Image.new('P', (cell_w, cell_h * rows), 0)
out.putpalette(latin_bmp.getpalette())
out.paste(latin_bmp.crop((0, 0, cell_w, cell_h * 94)), (0, 0))

# Butano treats the original old font as fixed-width when no width table is provided.
# Keep ASCII spacing identical by using 8px space + 8px ASCII glyph widths.
widths = [8] * 95

for row, cp in enumerate(range(0x0400, 0x0500), start=94):
    ent = struct.unpack_from('<H', pack, idx_off + (cp - start) * 2)[0]
    w = (ent >> 13) + 1
    off = ent & 0x1fff
    glyph = Image.new('P', (cell_w, cell_h), 0)
    glyph.putpalette(out.getpalette())
    pix = [0] * (cell_w * cell_h)
    for x in range(min(w, cell_w)):
        col = struct.unpack_from('<H', pack, chdata_off + (off + x) * 2)[0]
        for y in range(cell_h):
            if col & (1 << y):
                pix[y * cell_w + x] = 1
    glyph.putdata(pix)
    out.paste(glyph, (0, row * cell_h))
    widths.append(w + 1 if w < 8 else 8)  # SuperFW adds one spacing column for variable-width chars.

out.save(ROOT / 'graphics/vocab_superfw_cyrillic_font.bmp')
(ROOT / 'graphics/vocab_superfw_cyrillic_font.json').write_text('{\n    "type": "sprite",\n    "height": 16\n}\n')


def esc(ch):
    if ch == '\\':
        return '\\\\'
    if ch == '"':
        return '\\"'
    return ch

arr = ', '.join(f'"{esc(c)}"' for c in chars)
widths_s = ', '.join(str(int(w)) for w in widths)
header = f'''#ifndef VOCAB_SUPERFW_CYRILLIC_FONT_SPRITE_FONT_H
#define VOCAB_SUPERFW_CYRILLIC_FONT_SPRITE_FONT_H

#include "bn_sprite_font.h"
#include "bn_utf8_characters_map.h"
#include "bn_sprite_items_vocab_superfw_cyrillic_font.h"

constexpr bn::utf8_character vocab_superfw_cyrillic_font_sprite_font_utf8_characters[] = {{
    {arr}
}};

constexpr bn::span<const bn::utf8_character> vocab_superfw_cyrillic_font_sprite_font_utf8_characters_span(
        vocab_superfw_cyrillic_font_sprite_font_utf8_characters);

constexpr auto vocab_superfw_cyrillic_font_sprite_font_utf8_characters_map =
        bn::utf8_characters_map<vocab_superfw_cyrillic_font_sprite_font_utf8_characters_span>();

constexpr int8_t vocab_superfw_cyrillic_font_sprite_font_character_widths[] = {{
    {widths_s}
}};

constexpr bn::sprite_font vocab_superfw_cyrillic_font_sprite_font(
        bn::sprite_items::vocab_superfw_cyrillic_font,
        vocab_superfw_cyrillic_font_sprite_font_utf8_characters_map.reference(),
        vocab_superfw_cyrillic_font_sprite_font_character_widths);

#endif
'''
(ROOT / 'include/vocab_superfw_cyrillic_font_sprite_font.h').write_text(header, encoding='utf-8')
print('wrote SuperFW Cyrillic', out.size, 'chars', len(chars), 'max width', max(widths))
