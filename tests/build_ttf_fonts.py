#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageFont, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def write_font(name, font_path, size, extras, cell_w=16, cell_h=16, y_adjust=0, tight_arabic=False):
    font = ImageFont.truetype(font_path, size)
    chars = [chr(c) for c in range(0x21, 0x7f)] + list(dict.fromkeys(extras))
    strip = Image.new('P', (cell_w, cell_h * len(chars)), 0)
    strip.putpalette([0, 0, 0, 0, 0, 0] + [0, 0, 0] * 14)
    widths = []
    space_adv = int(round(font.getlength(' '))) if hasattr(font, 'getlength') else size // 2
    widths.append(max(3, min(cell_w, space_adv)))

    for i, ch in enumerate(chars):
        mask = Image.new('L', (cell_w * 3, cell_h * 3), 0)
        d = ImageDraw.Draw(mask)
        bbox = d.textbbox((0, 0), ch, font=font)
        gw = max(1, bbox[2] - bbox[0])
        gh = max(1, bbox[3] - bbox[1])
        x = max(0, (cell_w - gw) // 2 - bbox[0])
        y = max(0, (cell_h - gh) // 2 - bbox[1] + y_adjust)
        d.text((x, y), ch, font=font, fill=255)
        crop = mask.crop((0, 0, cell_w, cell_h))
        glyph = Image.new('P', (cell_w, cell_h), 0)
        glyph.putpalette(strip.getpalette())
        pix = [1 if v >= 96 else 0 for v in crop.getdata()]
        glyph.putdata(pix)
        strip.paste(glyph, (0, i * cell_h))

        pts = [(idx % cell_w, idx // cell_w) for idx, v in enumerate(pix) if v]
        if pts:
            minx = min(p[0] for p in pts)
            maxx = max(p[0] for p in pts)
            w = maxx - minx + 2
        else:
            w = space_adv
        cp = ord(ch)
        if tight_arabic and (0xFB50 <= cp <= 0xFEFF or 0x0600 <= cp <= 0x06FF):
            w = max(2, w - 4)
        widths.append(max(2, min(cell_w, w)))

    (ROOT / 'graphics' / f'{name}.bmp').parent.mkdir(exist_ok=True)
    strip.save(ROOT / 'graphics' / f'{name}.bmp')
    (ROOT / 'graphics' / f'{name}.json').write_text('{\n    "type": "sprite",\n    "height": 16\n}\n')

    utf_chars = chars[94:]

    def esc(ch):
        if ch == '\\':
            return '\\\\'
        if ch == '"':
            return '\\"'
        return ch

    arr = ', '.join(f'"{esc(c)}"' for c in utf_chars)
    widths_s = ', '.join(str(int(w)) for w in widths)
    guard = name.upper() + '_SPRITE_FONT_H'
    const = name + '_sprite_font'
    header = f'''#ifndef {guard}
#define {guard}

#include "bn_sprite_font.h"
#include "bn_utf8_characters_map.h"
#include "bn_sprite_items_{name}.h"

constexpr bn::utf8_character {const}_utf8_characters[] = {{
    {arr}
}};

constexpr bn::span<const bn::utf8_character> {const}_utf8_characters_span({const}_utf8_characters);

constexpr auto {const}_utf8_characters_map =
        bn::utf8_characters_map<{const}_utf8_characters_span>();

constexpr int8_t {const}_character_widths[] = {{
    {widths_s}
}};

constexpr bn::sprite_font {const}(
        bn::sprite_items::{name},
        {const}_utf8_characters_map.reference(),
        {const}_character_widths);

#endif
'''
    (ROOT / 'include' / f'{name}_sprite_font.h').write_text(header, encoding='utf-8')
    print(name, strip.size, len(widths), len(utf_chars))


cyr = ''.join(chr(c) for c in range(0x0400, 0x0500))
arab = ''.join(chr(c) for c in range(0x0600, 0x0700)) + ''.join(chr(c) for c in range(0xFB50, 0xFF00))
write_font('vocab_notosans_cyrillic_font', '/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf', 15, cyr, 16, 16, 0, False)
write_font('vocab_dejavu_arabic_font', '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 14, arab, 16, 16, 0, True)
