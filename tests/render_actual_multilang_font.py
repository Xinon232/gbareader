#!/usr/bin/env python3
import re
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / 'include/vocab_multilang_unscii_16_sprite_font.h').read_text(encoding='utf-8')
chars_block = header.split('vocab_multilang_unscii_16_sprite_font_utf8_characters[] = {', 1)[1].split('};', 1)[0]
chars = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', chars_block)
chars = [bytes(c, 'utf-8').decode('unicode_escape') if '\\' in c else c for c in chars]
widths_block = header.split('vocab_multilang_unscii_16_sprite_font_character_widths[] = {', 1)[1].split('};', 1)[0]
widths = [int(x) for x in re.findall(r'-?\d+', widths_block)]
char_to_pos = {c: i for i, c in enumerate(chars)}
font_p = Image.open(ROOT / 'graphics/vocab_multilang_unscii_16_font.bmp')
font = font_p.convert('RGBA')
cell_w, cell_h = font.size[0], 16
BG = (198,239,255,255)

def glyph_row(ch):
    cp = ord(ch)
    if ch == ' ':
        return None, widths[0]
    if 33 <= cp <= 126:
        return cp - 33, widths[cp - 32]
    pos = char_to_pos.get(ch)
    if pos is None:
        return None, 8
    return 94 + pos, widths[95 + pos]

def render_text(text, scale=3):
    glyphs=[]; total=0
    for ch in text:
        row,w = glyph_row(ch)
        glyphs.append((ch,row,w))
        total += max(w, 4)
    img = Image.new('RGBA', (max(1,total)*scale, cell_h*scale), BG)
    x=0
    for ch,row,w in glyphs:
        if row is not None:
            idx = font_p.crop((0, row*cell_h, cell_w, row*cell_h + cell_h))
            g = font.crop((0, row*cell_h, cell_w, row*cell_h + cell_h))
            data=[]
            for pal_i, px in zip(idx.getdata(), g.getdata()):
                if pal_i == 0:
                    data.append((0,0,0,0))
                else:
                    data.append(px)
            g.putdata(data)
            img.alpha_composite(g.resize((cell_w*scale, cell_h*scale), Image.Resampling.NEAREST), (x*scale,0))
        x += max(w, 4)
    return img.convert('RGB')

lines = ['Eichhörnchen ÄÖÜ äöü ß', 'дом русский', 'ﺀﺎﻣ ﻲﺑﺮﻋ']
out = Image.new('RGB', (760, 168), BG[:3])
y=8
for line in lines:
    im = render_text(line, 3)
    out.paste(im, (8, y))
    y += 52
out.save(ROOT / 'actual_multilang_font_preview.png')
print(ROOT / 'actual_multilang_font_preview.png')
