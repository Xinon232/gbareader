#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageDraw
import re

ROOT = Path(__file__).resolve().parents[1]
BG = (198,239,255,255)
BLACK = (0,0,0,255)

def parse_chars_and_widths(header_path, chars_name, widths_name=None):
    text = header_path.read_text(encoding='utf-8')
    chars = []
    if chars_name in text:
        block = text.split(chars_name, 1)[1].split('};', 1)[0]
        chars = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', block)
    widths = None
    if widths_name and widths_name in text:
        block = text.split(widths_name, 1)[1].split('};', 1)[0]
        widths = [int(x) for x in re.findall(r'-?\d+', block)]
    return chars, widths

latin_chars, _ = parse_chars_and_widths(
    ROOT / 'include/vocab_latin_old_ext_sprite_font.h',
    'latin_old_ext_sprite_font_utf8_characters[]')
superfw_cyr_chars, superfw_cyr_widths = parse_chars_and_widths(
    ROOT / 'include/vocab_superfw_cyrillic_font_sprite_font.h',
    'vocab_superfw_cyrillic_font_sprite_font_utf8_characters[]',
    'vocab_superfw_cyrillic_font_sprite_font_character_widths[]')
dejavu_ar_chars, dejavu_ar_widths = parse_chars_and_widths(
    ROOT / 'include/vocab_dejavu_arabic_font_sprite_font.h',
    'vocab_dejavu_arabic_font_sprite_font_utf8_characters[]',
    'vocab_dejavu_arabic_font_sprite_font_character_widths[]')

latin_map = {c: i for i, c in enumerate(latin_chars)}
superfw_cyr_map = {c: i for i, c in enumerate(superfw_cyr_chars)}
dejavu_ar_map = {c: i for i, c in enumerate(dejavu_ar_chars)}

def glyph_row(ch, char_map, widths, ascii_cell_width):
    cp = ord(ch)
    if ch == ' ':
        return None, widths[0] if widths else ascii_cell_width
    if 33 <= cp <= 126:
        return cp - 33, widths[cp - 32] if widths else ascii_cell_width
    pos = char_map.get(ch)
    if pos is None:
        pos = char_map.get('?')
        if pos is None:
            return None, ascii_cell_width
    return 94 + pos, widths[95 + pos] if widths else ascii_cell_width

def render_strip_text(text, bmp_path, char_map, widths=None, ascii_cell_width=8, scale=3):
    bmp_p = Image.open(bmp_path).convert('P')
    bmp_rgba = bmp_p.convert('RGBA')
    cell_w, cell_h = bmp_p.size[0], 16
    glyphs = []
    total = 0
    for ch in text:
        row, w = glyph_row(ch, char_map, widths, ascii_cell_width)
        glyphs.append((row, max(w, 4)))
        total += max(w, 4)
    img = Image.new('RGBA', (max(1,total) * scale, cell_h * scale), BG)
    x = 0
    for row, w in glyphs:
        if row is not None:
            idx = bmp_p.crop((0, row * cell_h, cell_w, row * cell_h + cell_h))
            g = bmp_rgba.crop((0, row * cell_h, cell_w, row * cell_h + cell_h))
            data = []
            for pal_i, px in zip(idx.getdata(), g.getdata()):
                data.append((0,0,0,0) if pal_i == 0 else BLACK)
            g.putdata(data)
            img.alpha_composite(g.resize((cell_w * scale, cell_h * scale), Image.Resampling.NEAREST), (x * scale, 0))
        x += w
    return img.convert('RGB')

rows = [
    ('OLD LATIN FONT', render_strip_text('Eichhörnchen ÄÖÜ äöü ß', ROOT/'graphics/vocab_latin_old_ext_font.bmp', latin_map, None, 8, 3)),
    ('SUPERFW RUSSIAN', render_strip_text('дом русский', ROOT/'graphics/vocab_superfw_cyrillic_font.bmp', superfw_cyr_map, superfw_cyr_widths, 8, 3)),
    ('DEJAVU ARABIC JOINED', render_strip_text('ﺐﺘﻛ  ﺀﺎﻣ  ﻲﺑﺮﻋ', ROOT/'graphics/vocab_dejavu_arabic_font.bmp', dejavu_ar_map, dejavu_ar_widths, 7, 3)),
]
W = 760
H = sum(max(52, im.height + 16) for _, im in rows) + 10
out = Image.new('RGB', (W, H), BG[:3])
d = ImageDraw.Draw(out)
y = 8
for label, im in rows:
    d.text((6, y + 10), label, fill=(0,0,0))
    out.paste(im, (180, y))
    y += max(52, im.height + 16)
out.save(ROOT / 'corrected_font_preview.png')
print(ROOT / 'corrected_font_preview.png')
