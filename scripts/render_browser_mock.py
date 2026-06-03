#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont

W, H = 240, 160
BG = (168, 216, 240)  # approximate 5-bit light blue
FG = (20, 50, 70)
SEL = (0, 0, 0)
img = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(img)
try:
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 13)
    title_font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 14)
except Exception:
    font = ImageFont.load_default()
    title_font = font

# Mirror Renderer::update_browser: centered text around x=0 maps to screen center.
def center_text(y, text, font, fill=FG):
    bbox = d.textbbox((0,0), text, font=font)
    x = (W - (bbox[2]-bbox[0])) // 2
    d.text((x, y), text, font=font, fill=fill)

center_text(12, 'Select TXT file', title_font)
center_text(32, 'A load   B cancel', font)
files = ['builtin.txt', 'NL-DE-5000.txt', 'ES-DE-vocab.txt']
for row, name in enumerate(files):
    text = ('> ' if row == 1 else '  ') + name
    y = 58 + row * 18
    fill = SEL if row == 1 else FG
    center_text(y, text, font, fill)

out = '/home/hlm/gba-vocab-v1-backup-20260602-210315/browser_layout_mock.png'
img.save(out)
print(out)
