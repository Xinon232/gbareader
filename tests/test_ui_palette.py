#!/usr/bin/env python3
"""Regression checks for the v0.2.5 white UI + light-blue UI font palette."""
from pathlib import Path
from hashlib import sha256
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
RENDER = (ROOT / "src" / "render.cpp").read_text()
UI_FONT = ROOT / "graphics" / "ui_variable_8x16_font.bmp"
ORIGINAL_COLORS = [
    (0, 255, 0), (49, 65, 95), (108, 56, 90), (176, 50, 67),
    (225, 57, 57), (96, 97, 97), (73, 101, 130), (240, 101, 70),
    (98, 131, 191), (97, 142, 188), (255, 146, 83), (134, 184, 165),
    (0, 0, 0), (188, 193, 195), (255, 255, 255), (15, 0, 0),
]
ORIGINAL_PIXEL_SHA256 = "fc5be27a030bf8a6e096c08b7bcd46fbf84da392cf80787d2a597ad213f236ab"

assert "constexpr int BG_R = 31;" in RENDER
assert "constexpr int BG_G = 31;" in RENDER
assert "constexpr int BG_B = 31;" in RENDER
assert UI_FONT.exists(), "v0.2.5 UI palette asset is missing"

ui = Image.open(UI_FONT)
assert ui.mode == "P"
assert ui.size == (8, 1760)
assert sha256(ui.tobytes()).hexdigest() == ORIGINAL_PIXEL_SHA256, "UI font pixel/index data changed"

palette = ui.getpalette()
assert palette is not None
ui_palette = palette[:48]
for index in range(16):
    start = index * 3
    source_color = ORIGINAL_COLORS[index]
    ui_color = tuple(ui_palette[start:start + 3])
    if index == 14:  # the source font's white foreground
        assert ui_color == (198, 247, 255), ui_color  # exact GBA RGB5(24,30,31)
    else:
        assert ui_color == source_color, (index, source_color, ui_color)

assert tuple(ui_palette[12 * 3:12 * 3 + 3]) == (0, 0, 0), "black font pixels changed"
print("PASS: white background and exact light-blue UI font palette")
