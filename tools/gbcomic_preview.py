#!/usr/bin/env python3
"""What the GBA will show, before converting: one page of a comic at one zoom,
made by the converter's own code (gbcomic.py: crop, scale, sharpen, 248
colours), cut into screens and read with the reader's rules (comic_layout.cpp,
comic_reader.cpp): the fixed grid, A / B from screen to screen, the D-pad
moving the view, Skip empty and the page map. Pure Python and Pillow; the
converter window (gbcomic_gui.py) shows it.
"""
import io
import os
import shutil
import subprocess
import tempfile
import zipfile

from PIL import Image, ImageDraw

import gbcomic

SCREEN = {0: (240, 160), 1: (160, 240)}     # as the reader sees it: upright, sideways
MAX_ALONG = 24
LEFT_TO_RIGHT, RIGHT_TO_LEFT, COLUMNS = 0, 1, 2
ORDER_NAMES = ['Left to right', 'Right to left', 'Columns']
MAP_SIZE = 44
# The reader's own colours (reader_screen.h).
WHITE, BLACK, BLUE, GREY = (255, 255, 255), (0, 0, 0), (170, 210, 255), (132, 132, 132)


# ---------------------------------------------------------------- pages, one at a time

class PageSource:
    """The pages of a comic, any one on request (the converter reads them in order)."""

    def __init__(self, source, dpi=200):
        self.source, self.dpi = source, dpi
        self.names, self.pdf, self.kind = [], None, 'picture'
        if os.path.isdir(source):
            self.kind = 'folder'
            self.names = sorted((n for n in os.listdir(source) if gbcomic.is_picture(n)), key=gbcomic.natural_key)
        elif source.lower().endswith('.pdf'):
            self.kind = 'pdf'
            try:
                import pypdfium2 as pdfium
                self.pdf = pdfium.PdfDocument(source)
                self.names = [f'page {i + 1}' for i in range(len(self.pdf))]
            except ImportError:
                self.names = [f'page {i + 1}' for i in range(gbcomic.count_pages(source))]
        elif zipfile.is_zipfile(source):
            self.kind = 'zip'
            with zipfile.ZipFile(source) as z:
                if source.lower().endswith('.epub') or 'META-INF/container.xml' in z.namelist():
                    self.names = gbcomic.epub_pictures(z)
                else:
                    self.names = sorted((n for n in z.namelist() if gbcomic.is_picture(n)), key=gbcomic.natural_key)
        else:
            self.names = [os.path.basename(source)]

    def count(self):
        return len(self.names)

    def page(self, index):
        """Page index (0-based) as an RGB picture."""
        if self.kind == 'folder':
            with Image.open(os.path.join(self.source, self.names[index])) as im:
                return im.convert('RGB')
        if self.kind == 'zip':
            with zipfile.ZipFile(self.source) as z, Image.open(io.BytesIO(z.read(self.names[index]))) as im:
                return im.convert('RGB')
        if self.kind == 'pdf':
            if self.pdf is not None:
                page = self.pdf[index]
                try:
                    return page.render(scale=self.dpi / 72).to_pil().convert('RGB')
                finally:
                    page.close()
            if not shutil.which('pdftoppm'):
                raise RuntimeError('PDF needs pypdfium2 or pdftoppm')
            with tempfile.TemporaryDirectory() as folder:
                subprocess.run(['pdftoppm', '-r', str(self.dpi), '-f', str(index + 1), '-l', str(index + 1),
                                '-png', self.source, os.path.join(folder, 'p')], check=True)
                name = sorted(os.listdir(folder))[0]
                with Image.open(os.path.join(folder, name)) as im:
                    return im.convert('RGB')
        with Image.open(self.source) as im:
            return im.convert('RGB')

    def close(self):
        if self.pdf is not None:
            self.pdf.close()
            self.pdf = None


def view_pictures(page, orientation, kind, factor, options):
    """The pictures of one page at one zoom, exactly as the converter makes them."""
    one = gbcomic_options(options, [(orientation, kind, factor)])
    return gbcomic.views_for_page(page, one)[0][1]


def gbcomic_options(options, zooms):
    class O:
        pass
    o = O()
    for key in ('sharpen', 'margins', 'spreads', 'dither', 'boost_lettering'):
        setattr(o, key, getattr(options, key))
    o.zoom_list = zooms
    return o


# ---------------------------------------------------------------- the screens (comic_layout.cpp)

def auto_count(span, size):
    if span <= size:
        return 1
    if span % size == 0:
        return span // size
    return 1 + (span - size + (size - 16) - 1) // (size - 16)


def spread(pic, size, count=0):
    if pic <= size:
        return [-((size - pic) // 2)]
    last = pic - size
    n = min(count or auto_count(pic, size), MAX_ALONG)
    left_over = pic - n * size
    out = []
    for i in range(n):
        if left_over <= 0:
            v = (last * i * 2 + (n - 1)) // (2 * (n - 1))
        else:
            v = i * size + (left_over * (i + 1) * 2 + (n + 1)) // (2 * (n + 1))
        out.append(max(0, min(last, v)))
    return out


class Grid:
    def __init__(self, pic_w, pic_h, sw, sh, columns=0, rows=0):
        self.pic_w, self.pic_h, self.sw, self.sh = pic_w, pic_h, sw, sh
        self.xs, self.ys = spread(pic_w, sw, columns), spread(pic_h, sh, rows)

    def limits(self):
        if self.pic_w > self.sw:
            min_x, max_x = 0, self.pic_w - self.sw
        else:
            min_x = max_x = -((self.sw - self.pic_w) // 2)
        if self.pic_h > self.sh:
            min_y, max_y = 0, self.pic_h - self.sh
        else:
            min_y = max_y = -((self.sh - self.pic_h) // 2)
        return min_x, max_x, min_y, max_y


def spot_at(grids, order, index):
    """(picture, column, row) of screen index in reading order."""
    for i, g in enumerate(grids):
        n = len(g.xs) * len(g.ys)
        if index >= n and i + 1 < len(grids):
            index -= n
            continue
        index = max(0, min(n - 1, index))
        if order == COLUMNS:
            return i, index // len(g.ys), index % len(g.ys)
        row, column = index // len(g.xs), index % len(g.xs)
        if order == RIGHT_TO_LEFT:
            column = len(g.xs) - 1 - column
        return i, column, row
    return 0, 0, 0


def spot_index(grids, order, spot):
    picture, column, row = spot
    index = sum(len(g.xs) * len(g.ys) for g in grids[:picture])
    g = grids[picture]
    if order == COLUMNS:
        return index + column * len(g.ys) + row
    if order == RIGHT_TO_LEFT:
        column = len(g.xs) - 1 - column
    return index + row * len(g.xs) + column


def any_ink(picture, x, y, w, h):
    """Any ink under the screen at (x, y) (from the picture's 8 x 8 ink grid)."""
    cw, ch = picture.grid_w, picture.grid_h
    cell = gbcomic.CELL
    x0, y0 = max(0, min(cw, x // cell if x >= 0 else 0)), max(0, min(ch, y // cell if y >= 0 else 0))
    x1 = 0 if x + w < 0 else max(0, min(cw, (x + w + cell - 1) // cell))
    y1 = 0 if y + h < 0 else max(0, min(ch, (y + h + cell - 1) // cell))
    for cy in range(y0, y1):
        for cx in range(x0, x1):
            i = cy * cw + cx
            if picture.ink[i >> 3] & (1 << (i & 7)):
                return True
    return False


# ---------------------------------------------------------------- reading a page (comic_reader.cpp)

class PageView:
    """One page at one zoom as the reader shows it: which screen, where the
    view is, and the screen drawn with the page map."""

    def __init__(self, pictures, page_size, orientation, order=LEFT_TO_RIGHT, skip_empty=True, counts=(0, 0)):
        self.pictures = pictures
        self.page_w, self.page_h = page_size
        self.sw, self.sh = SCREEN[orientation]
        self.order = order
        self.skip_empty = skip_empty
        one = len(pictures) == 1
        self.grids = [Grid(p.w, p.h, self.sw, self.sh, counts[0] if one else 0, counts[1] if one else 0)
                      for p in pictures]
        self.tiles = sum(len(g.xs) * len(g.ys) for g in self.grids)
        self.cells = one
        self.rgb = [None] * len(pictures)

    # Where things are.
    def tile_xy(self, tile):
        p, c, r = spot_at(self.grids, self.order, tile)
        return p, self.grids[p].xs[c], self.grids[p].ys[r]

    def skip(self, tile):
        if not self.cells or not self.skip_empty:
            return False
        _, x, y = self.tile_xy(tile)
        return not any_ink(self.pictures[0], x, y, self.sw, self.sh)

    def open(self, at_end=False):
        if at_end:
            self.tile = self.tiles - 1
            while self.tile > 0 and self.skip(self.tile):
                self.tile -= 1
        else:
            self.tile = 0
            while self.tile + 1 < self.tiles and self.skip(self.tile):
                self.tile += 1
        self.spot = spot_at(self.grids, self.order, self.tile)
        _, self.x, self.y = self.tile_xy(self.tile)

    def empty_at(self, spot, x, y):
        return (self.cells and self.skip_empty and spot[0] == 0 and
                not any_ink(self.pictures[0], x, y, self.sw, self.sh))

    def next_place(self, spot, x, y, direction):
        """('along' | 'line' | 'picture' | 'end', spot, x, y): the reader's A / B."""
        down = self.order == COLUMNS
        picture, column, row = spot
        g = self.grids[picture]
        along, across = (g.ys, g.xs) if down else (g.xs, g.ys)
        count, lines = (len(g.ys), len(g.xs)) if down else (len(g.xs), len(g.ys))
        size = self.sw if down else self.sh
        at, cross = (y, x) if down else (x, y)
        reading = 1 if down or self.order == LEFT_TO_RIGHT else -1
        want = reading * direction
        here = row if down else column
        k = -1
        if want > 0:
            k = next((i for i in range(here + 1, count) if along[i] > at), -1)
        else:
            k = next((i for i in range(here - 1, -1, -1) if along[i] < at), -1)
        if k >= 0:
            if down:
                return 'along', (picture, column, k), x, along[k]
            return 'along', (picture, k, row), along[k], y
        line = (column if down else row) + direction
        if 0 <= line < lines:
            first, last = (0, count - 1) if reading > 0 else (count - 1, 0)
            index = first if direction > 0 else last
            grid = across[line]
            c = max(grid, min(grid + size // 2, cross)) if direction > 0 else max(grid - size // 2, min(grid, cross))
            min_x, max_x, min_y, max_y = g.limits()
            c = max(min_x, min(max_x, c)) if down else max(min_y, min(max_y, c))
            if down:
                return 'line', (picture, line, index), c, along[index]
            return 'line', (picture, index, line), along[index], c
        picture += direction
        if picture < 0 or picture >= len(self.grids):
            return 'end', spot, x, y
        index = sum(len(gg.xs) * len(gg.ys) for gg in self.grids[:picture])
        if direction < 0:
            index += len(self.grids[picture].xs) * len(self.grids[picture].ys) - 1
        spot = spot_at(self.grids, self.order, index)
        return 'picture', spot, self.grids[picture].xs[spot[1]], self.grids[picture].ys[spot[2]]

    def step(self, direction):
        """A (1) or B (-1): 'moved', 'next page' or 'previous page'."""
        spot, x, y = self.spot, self.x, self.y
        for _ in range(self.tiles + 1):
            reach, spot, x, y = self.next_place(spot, x, y, direction)
            if reach == 'end':
                return 'next page' if direction > 0 else 'previous page'
            if not self.empty_at(spot, x, y):
                break
        self.spot, self.x, self.y = spot, x, y
        self.tile = spot_index(self.grids, self.order, spot)
        return 'moved'

    def move_by(self, dx, dy):
        """The D-pad: the view goes (dx, dy) further, up to the picture's edges."""
        min_x, max_x, min_y, max_y = self.grids[self.spot[0]].limits()
        self.x = max(min_x, min(max_x, self.x + dx))
        self.y = max(min_y, min(max_y, self.y + dy))

    # Drawing.
    def picture_rgb(self, i):
        if self.rgb[i] is None:
            p = self.pictures[i]
            five = [((c & 31) << 3 | (c & 31) >> 2, (c >> 5 & 31) << 3 | (c >> 5 & 31) >> 2,
                     (c >> 10 & 31) << 3 | (c >> 10 & 31) >> 2) for c in p.palette]
            flat = [WHITE] * gbcomic.FIRST + five
            im = Image.frombytes('P', (p.w, p.h), p.pixels)
            im.putpalette([v for c in flat for v in c] + [0] * (768 - 3 * len(flat)))
            self.rgb[i] = im.convert('RGB')
        return self.rgb[i]

    def page_box(self, picture, x, y):
        p = self.pictures[picture]
        x0, x1 = max(0, x), min(p.w, x + self.sw)
        y0, y1 = max(0, y), min(p.h, y + self.sh)
        bw, bh = p.box[2] - p.box[0], p.box[3] - p.box[1]
        return (p.box[0] + bw * x0 // p.w, p.box[1] + bh * y0 // p.h,
                p.box[0] + bw * x1 // p.w, p.box[1] + bh * y1 // p.h)

    def map_rect(self):
        pw, ph = self.page_w, self.page_h
        w = MAP_SIZE if pw >= ph else MAP_SIZE * pw // ph
        h = MAP_SIZE * ph // pw if pw >= ph else MAP_SIZE
        w, h = max(w, 4), max(h, 4)
        return self.sw - w - 9, 3, w + 6, h + 6

    def render(self, page_map=True):
        """The screen (sw x sh) as the reader draws it."""
        screen = Image.new('RGB', (self.sw, self.sh), WHITE)
        picture = self.spot[0]
        screen.paste(self.picture_rgb(picture), (-self.x, -self.y))
        if page_map and self.tiles > 1:
            d = ImageDraw.Draw(screen)

            def fill(colour, x, y, w, h):
                if w > 0 and h > 0:
                    d.rectangle([x, y, x + w - 1, y + h - 1], fill=colour)
            mx, my, mw, mh = self.map_rect()
            x0, y0, w, h = mx + 3, my + 3, mw - 6, mh - 6
            fill(BLACK, mx, my, mw, mh)
            fill(WHITE, x0 - 2, y0 - 2, w + 4, h + 4)
            fill(GREY, x0, y0, w, h)
            fill(WHITE, x0 + 1, y0 + 1, w - 2, h - 2)
            b = self.page_box(picture, self.x, self.y)
            iw, ih = w - 2, h - 2
            tw, th = max(2, (b[2] - b[0]) * iw // 4096), max(2, (b[3] - b[1]) * ih // 4096)
            tx = max(0, min(iw - tw, b[0] * iw // 4096)) + x0 + 1
            ty = max(0, min(ih - th, b[1] * ih // 4096)) + y0 + 1
            fill(BLACK, tx, ty, tw, th)
            if tw > 4 and th > 4:
                fill(BLUE, tx + 1, ty + 1, tw - 2, th - 2)
        return screen
