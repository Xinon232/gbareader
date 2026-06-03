#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageDraw
import re

ROOT = Path(__file__).resolve().parents[1]
BG = (198,239,255,255)
BLACK = (0,0,0,255)


def parse_chars_and_widths(header_path, stem):
    text = header_path.read_text(encoding='utf-8')
    chars_block = text.split(f'{stem}_sprite_font_utf8_characters[]', 1)[1].split('};', 1)[0]
    chars = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', chars_block)
    widths_block = text.split(f'{stem}_sprite_font_character_widths[]', 1)[1].split('};', 1)[0]
    widths = [int(x) for x in re.findall(r'-?\d+', widths_block)]
    return {c: i for i, c in enumerate(chars)}, widths


def render_strip_text(text, bmp_path, char_map, widths, scale=3):
    bmp_p = Image.open(bmp_path).convert('P')
    bmp_rgba = bmp_p.convert('RGBA')
    cell_w, cell_h = bmp_p.size[0], 16
    glyphs = []
    total = 0
    for ch in text:
        cp = ord(ch)
        if ch == ' ':
            row = None
            w = widths[0]
        elif 33 <= cp <= 126:
            row = cp - 33
            w = widths[cp - 32]
        else:
            pos = char_map.get(ch)
            if pos is None:
                row = None
                w = widths[0]
            else:
                row = 94 + pos
                w = widths[95 + pos]
        glyphs.append((row, max(w, 1)))
        total += max(w, 1)
    img = Image.new('RGBA', (max(1,total) * scale, cell_h * scale), BG)
    x = 0
    for row, w in glyphs:
        if row is not None:
            idx = bmp_p.crop((0, row * cell_h, cell_w, row * cell_h + cell_h))
            g = bmp_rgba.crop((0, row * cell_h, cell_w, row * cell_h + cell_h))
            g.putdata([(0,0,0,0) if pal_i == 0 else BLACK for pal_i in idx.getdata()])
            img.alpha_composite(g.resize((cell_w * scale, cell_h * scale), Image.Resampling.NEAREST), (x * scale, 0))
        x += w
    return img.convert('RGB')

cmap_r, widths_r = parse_chars_and_widths(ROOT/'include/vocab_notosans_cyrillic_font_sprite_font.h', 'vocab_notosans_cyrillic_font')
cmap_a, widths_a = parse_chars_and_widths(ROOT/'include/vocab_dejavu_arabic_font_sprite_font.h', 'vocab_dejavu_arabic_font')
rows = [
    ('NOTO SANS RUSSIAN', render_strip_text('дом русский', ROOT/'graphics/vocab_notosans_cyrillic_font.bmp', cmap_r, widths_r, 4)),
    ('DEJAVU ARABIC JOINED', render_strip_text('ﺐﺘﻛ  ﺀﺎﻣ  ﻲﺑﺮﻋ', ROOT/'graphics/vocab_dejavu_arabic_font.bmp', cmap_a, widths_a, 4)),
]
out = Image.new('RGB', (760, 150), BG[:3])
d = ImageDraw.Draw(out)
y = 12
for label, im in rows:
    d.text((6, y + 18), label, fill=(0,0,0))
    out.paste(im, (185, y))
    y += 66
out.save(ROOT/'noto_russian_arabic_preview.png')
print(ROOT/'noto_russian_arabic_preview.png')
