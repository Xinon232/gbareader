#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont

W, H = 240, 160
BG = (198, 239, 255)      # approx GBA 5-bit RGB (24,30,31)
UI = (255, 255, 255)      # original UI font color
FLASHCARD = (255, 255, 255)

Y_MODE = -64
Y_HEADER = -44
Y_PROMPT = -20
Y_ANSWER = 12
Y_FOOTER = 64
SAVE_X = 116
SAVE_Y = -72
WRAP_MAX_CHARS = 27
WRAP_LINE_STEP = 12

prompt = 'ardilla {f} Eichhörnchen дом'
answer = 'burbuja {f} glóbulo ﺀﺎﻣ'

def sy(y):
    return 80 + y

def sx(x):
    return 120 + x

def wrap_text(text):
    lines = []
    pos = 0
    while pos < len(text) and len(lines) < 2:
        while pos < len(text) and text[pos] == ' ':
            pos += 1
        if pos >= len(text):
            break
        remaining_lines = 2 - len(lines)
        remaining_chars = len(text) - pos
        take = remaining_chars
        if remaining_lines > 1 and remaining_chars > WRAP_MAX_CHARS:
            limit = pos + WRAP_MAX_CHARS
            break_pos = -1
            for i in range(limit, pos, -1):
                if text[i] == ' ':
                    break_pos = i
                    break
            if break_pos <= pos:
                break_pos = limit
            take = break_pos - pos
        lines.append(text[pos:pos+take].strip())
        pos += take
    return lines

def draw_center(draw, y, text, fill, font):
    bbox = draw.textbbox((0, 0), text, font=font)
    x = (W - (bbox[2] - bbox[0])) // 2
    draw.text((x, sy(y) - (bbox[3] - bbox[1]) // 2), text, fill=fill, font=font)

def draw_right(draw, x, y, text, fill, font):
    bbox = draw.textbbox((0, 0), text, font=font)
    draw.text((sx(x) - (bbox[2] - bbox[0]), sy(y) - (bbox[3] - bbox[1]) // 2), text, fill=fill, font=font)

def draw_wrapped(draw, base_y, text, fill, font):
    lines = wrap_text(text)
    start_y = base_y - ((len(lines) - 1) * WRAP_LINE_STEP) // 2
    for i, line in enumerate(lines):
        draw_center(draw, start_y + i * WRAP_LINE_STEP, line, fill, font)

font_ui = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 8)
font_big = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 10)
img = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(img)

draw_center(d, Y_MODE, 'Mode 1', UI, font_ui)
draw_right(d, SAVE_X, SAVE_Y, 'save...', UI, font_ui)
draw_center(d, Y_HEADER, 'Field 1/5 - 5000 words', UI, font_ui)
draw_wrapped(d, Y_PROMPT, prompt, FLASHCARD, font_big)
draw_wrapped(d, Y_ANSWER, answer, FLASHCARD, font_big)
for x, label in [(-96,'F1:1000'),(-48,'F2:1000'),(0,'F3:1000'),(48,'F4:1000'),(96,'F5:1000')]:
    bbox = d.textbbox((0, 0), label, font=font_ui)
    d.text((sx(x) - (bbox[2] - bbox[0]) // 2, sy(Y_FOOTER) - (bbox[3] - bbox[1]) // 2), label, fill=UI, font=font_ui)

img.save('long_item_save_grey_mock.png')
print('long_item_save_grey_mock.png')
