#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont

SCALE = 3
W, H = 240, 160
BG_5BIT = (24, 30, 31)
BG = tuple(round(v * 255 / 31) for v in BG_5BIT)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)

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

def field_label(text, x_center, y_center):
    bbox = d.textbbox((0, 0), text, font=font_small)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = W // 2 + x_center - tw // 2
    y = H // 2 + y_center - th // 2
    d.text((x, y), text, font=font_small, fill=WHITE)

center('Mode 1', -64, font_small)
center('Field 2/5 - 123 words', -44, font_small)
center('EMPTY', -20, font_big)

footer_x = [-96, -48, 0, 48, 96]
labels = ['F1:10', 'F2:0', 'F3:30', 'F4:40', 'F5:50']
for label, x in zip(labels, footer_x):
    field_label(label, x, 64)

# Current field = 2; black underline below F2.
x = W // 2 + footer_x[1]
y = H // 2 + 64 + 12
d.rectangle((x - 16, y - 1, x + 15, y), fill=BLACK)

out = '/home/hlm/gba-vocab-v1-backup-20260602-210315/field_underline_empty_box_mock.png'
img.resize((W*SCALE, H*SCALE), Image.Resampling.NEAREST).save(out)
print(out)
