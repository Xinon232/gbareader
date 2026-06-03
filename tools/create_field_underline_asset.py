#!/usr/bin/env python3
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
img = Image.new('P', (8, 8), 0)
# Palette index 0 is transparent for sprites; keep it bright green for debugging.
# Palette index 1 is the visible black underline.
palette = [0, 255, 0, 0, 0, 0] + [0, 0, 0] * 254
img.putpalette(palette)
px = img.load()
for y in (3, 4):
    for x in range(8):
        px[x, y] = 1
out = root / 'graphics' / 'field_underline.bmp'
img.save(out)
print(out)
