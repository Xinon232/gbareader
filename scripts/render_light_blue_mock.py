#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont

SCALE = 3
W, H = 240, 160
BG_5BIT = (24, 30, 31)
BG = tuple(round(v * 255 / 31) for v in BG_5BIT)
WHITE = (255, 255, 255)

img = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(img)
font_small = ImageFont.load_default()
font_big = ImageFont.load_default(size=16)

def center(text, y_center, font):
    bbox = d.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (W - tw) // 2
    y = (H // 2 + y_center) - th // 2
    d.text((x, y), text, font=font, fill=WHITE)

center('Mode 1', -64, font_small)
center('Field 1/5 - 123 words', -44, font_small)
center('bonjour', -20, font_big)
center('hello', 12, font_big)
# Intentionally no L1>R2 / L2>R1 line at y=44.
center('F1:10 F2:20 F3:30 F4:40 F5:50', 64, font_small)

out = '/home/hlm/gba-vocab-v1-backup-20260602-210315/light_blue_layout_mock.png'
img.resize((W*SCALE, H*SCALE), Image.Resampling.NEAREST).save(out)
print(out)
