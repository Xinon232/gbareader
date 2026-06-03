#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageDraw
import re

ROOT = Path(__file__).resolve().parents[1]
BG = (198,239,255,255)
BLACK = (0,0,0,255)


def parse_font(header_path, stem):
    text = header_path.read_text(encoding='utf-8')
    chars_block = text.split(f'{stem}_sprite_font_utf8_characters[]', 1)[1].split('};', 1)[0]
    chars = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', chars_block)
    widths_block = text.split(f'{stem}_sprite_font_character_widths[]', 1)[1].split('};', 1)[0]
    widths = [int(x) for x in re.findall(r'-?\d+', widths_block)]
    return {c:i for i,c in enumerate(chars)}, widths


def render(text, bmp_path, cmap, widths, scale=4):
    bmp_p = Image.open(bmp_path).convert('P')
    bmp_rgba = bmp_p.convert('RGBA')
    cell_w, cell_h = bmp_p.size[0], 16
    glyphs=[]; total=0
    for ch in text:
        cp=ord(ch)
        if ch == ' ':
            row=None; w=widths[0]
        elif 33 <= cp <= 126:
            row=cp-33; w=widths[cp-32]
        else:
            pos=cmap.get(ch)
            row=None if pos is None else 94+pos
            w=widths[95+pos] if pos is not None else widths[0]
        glyphs.append((row,w)); total += max(1,w)
    out=Image.new('RGBA',(max(1,total)*scale,cell_h*scale),BG)
    x=0
    for row,w in glyphs:
        if row is not None:
            idx=bmp_p.crop((0,row*cell_h,cell_w,row*cell_h+cell_h))
            g=bmp_rgba.crop((0,row*cell_h,cell_w,row*cell_h+cell_h))
            g.putdata([(0,0,0,0) if p==0 else BLACK for p in idx.getdata()])
            out.alpha_composite(g.resize((cell_w*scale,cell_h*scale), Image.Resampling.NEAREST), (x*scale,0))
        x += max(1,w)
    return out.convert('RGB')

cmap,widths = parse_font(ROOT/'include/vocab_superfw_cyrillic_font_sprite_font.h', 'vocab_superfw_cyrillic_font')
rows=[
    ('SUPERFW CYRILLIC', render('дом русский медведь', ROOT/'graphics/vocab_superfw_cyrillic_font.bmp', cmap, widths, 4)),
    ('SUPERFW CYRILLIC CAPS', render('АБВГД ЕЖЗИЙ', ROOT/'graphics/vocab_superfw_cyrillic_font.bmp', cmap, widths, 4)),
]
out=Image.new('RGB',(760,150),BG[:3])
d=ImageDraw.Draw(out)
y=12
for label,img in rows:
    d.text((6,y+20),label,fill=(0,0,0))
    out.paste(img,(185,y))
    y += 66
out.save(ROOT/'superfw_russian_preview.png')
print(ROOT/'superfw_russian_preview.png')
