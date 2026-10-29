#!/usr/bin/env python3
"""Converts a comic into a .gbcomic file for gbareader (Game Boy Advance).

Every picture the GBA shows is prepared here, once: each page is cropped,
scaled with a good filter straight from the original for each zoom of each
screen angle, sharpened and given its own 248 colours. The GBA then only
reads and unpacks the part it shows, and places the screens itself.

Input: a PDF, an EPUB (comic: its pictures in reading order), a CBZ/ZIP of
pictures, a folder of pictures or one picture. Needs Python 3.8+ and Pillow;
PDF needs pypdfium2 (pip install pypdfium2) or pdftoppm (poppler-utils).

    python3 gbcomic.py "My Comic.cbz"                 -> My Comic.gbcomic
    python3 gbcomic.py book.pdf -o "Book.gbcomic" --angles sideways --sideways 2.3,2.5
    python3 gbcomic.py --ask a.pdf b.epub             asks for the angles and zooms first

The ready-made Windows and Linux packages run it with --ask.

The file format is described in docs/gbcomic-format.md.
"""
import argparse
import io
import os
import posixpath
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from urllib.parse import unquote
from xml.etree import ElementTree

from PIL import Image, ImageChops, ImageFilter

VERSION = 2
COLORS = 248            # screen colours 8..255; 0..7 belong to the reader
FIRST = 8
CHUNK_ROWS = 16
CELL = 8                # ink grid: one bit per 8 x 8 pixels
PANEL_UNIT = 4096
ORIENTATIONS = [(240, 160), (160, 240)]                         # 0 upright, 1 sideways
KIND_FIT_PAGE, KIND_WIDTH = 0, 1
DEFAULT_NORMAL = '1.6,1.8,2.0'
DEFAULT_SIDEWAYS = '2.2,2.4,2.6'
# The zooms a file can have, in hundredths: upright (Normal, Upside down) and sideways (90°).
# 1.0 is Fit width, always in the file (typing it adds nothing). Any number of
# them: with Fit page and Fit width at most 44 zooms (the reader takes 48).
NORMAL_CHOICES = list(range(110, 301, 10))
SIDEWAYS_CHOICES = list(range(110, 301, 10))
MAX_PER_ANGLE = len(NORMAL_CHOICES)
PICTURE_EXTENSIONS = ('.jpg', '.jpeg', '.jpe', '.jfif', '.png', '.gif', '.webp', '.bmp', '.tif', '.tiff')


# ---------------------------------------------------------------- input

def natural_key(name):
    return [int(part) if part.isdigit() else part.lower() for part in re.split(r'(\d+)', name)]


def is_picture(name):
    base = os.path.basename(name)
    return (name.lower().endswith(PICTURE_EXTENSIONS) and not base.startswith('.')
            and not name.startswith('__MACOSX/'))


def epub_pictures(z):
    """The pictures of a (comic) EPUB in reading order: the spine's documents
    and the pictures they show; every picture in the book if that finds none."""
    names = set(z.namelist())
    found = []
    try:
        container = ElementTree.fromstring(z.read('META-INF/container.xml'))
        opf_path = next(e.get('full-path') for e in container.iter() if e.tag.endswith('rootfile'))
        opf = ElementTree.fromstring(z.read(opf_path))
        base = posixpath.dirname(opf_path)
        manifest = {}
        for e in opf.iter():
            if e.tag.endswith('}item') or e.tag == 'item':
                manifest[e.get('id')] = (posixpath.normpath(posixpath.join(base, unquote(e.get('href', '')))),
                                         e.get('media-type', ''))
        spine = [e.get('idref') for e in opf.iter() if e.tag.endswith('itemref') or e.tag == 'itemref']
        for idref in spine:
            path, media = manifest.get(idref, ('', ''))
            if path not in names:
                continue
            if is_picture(path):
                found.append(path)
                continue
            text = z.read(path).decode('utf-8', 'replace')
            folder = posixpath.dirname(path)
            for ref in re.findall(r'<(?:\w+:)?(?:img|image)\b[^>]*?\b(?:src|xlink:href|href)\s*=\s*["\']([^"\']+)', text, re.I):
                target = posixpath.normpath(posixpath.join(folder, unquote(ref.split('#')[0])))
                if target in names and is_picture(target) and target not in found:
                    found.append(target)
    except (KeyError, StopIteration, ElementTree.ParseError):
        found = []
    return found or sorted((n for n in names if is_picture(n)), key=natural_key)


def pdf_pages(source, dpi):
    """PDF pages as pictures: pypdfium2 if it is there, else pdftoppm."""
    try:
        import pypdfium2 as pdfium
    except ImportError:
        pdfium = None
    if pdfium:
        pdf = pdfium.PdfDocument(source)
        try:
            for i in range(len(pdf)):
                page = pdf[i]
                im = page.render(scale=dpi / 72).to_pil().convert('RGB')
                page.close()
                yield f'page {i + 1}', im
        finally:
            pdf.close()
        return
    if not shutil.which('pdftoppm'):
        sys.exit('PDF input needs pypdfium2 (pip install pypdfium2) or pdftoppm (poppler-utils).')
    with tempfile.TemporaryDirectory() as folder:
        subprocess.run(['pdftoppm', '-r', str(dpi), '-png', source, os.path.join(folder, 'p')], check=True)
        for name in sorted(os.listdir(folder), key=natural_key):
            with Image.open(os.path.join(folder, name)) as im:
                yield name, im.convert('RGB')


def count_pages(source):
    """How many pages load_pages will give (0 if it cannot tell)."""
    try:
        if os.path.isdir(source):
            return sum(1 for n in os.listdir(source) if is_picture(n))
        if source.lower().endswith('.pdf'):
            try:
                import pypdfium2 as pdfium
                pdf = pdfium.PdfDocument(source)
                n = len(pdf)
                pdf.close()
                return n
            except ImportError:
                return 0
        if zipfile.is_zipfile(source):
            with zipfile.ZipFile(source) as z:
                if source.lower().endswith('.epub') or 'META-INF/container.xml' in z.namelist():
                    return len(epub_pictures(z))
                return sum(1 for n in z.namelist() if is_picture(n))
        return 1
    except Exception:
        return 0


def zoom_list(angles, normal, sideways):
    """The file's zooms for the angles chosen and the zoom values (hundredths)."""
    result = []
    for orientation in {'both': [0, 1], 'upright': [0], 'sideways': [1]}[angles]:
        result.append((orientation, KIND_FIT_PAGE, 0))
        result.append((orientation, KIND_WIDTH, 100))
        result += [(orientation, KIND_WIDTH, f) for f in (normal if orientation == 0 else sideways)]
    return result


def load_pages(source, dpi):
    """The pages as RGB pictures, in reading order (yields them one at a time)."""
    if os.path.isdir(source):
        names = sorted((n for n in os.listdir(source) if is_picture(n)), key=natural_key)
        for name in names:
            with Image.open(os.path.join(source, name)) as im:
                yield name, im.convert('RGB')
    elif source.lower().endswith('.pdf'):
        yield from pdf_pages(source, dpi)
    elif zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as z:
            if source.lower().endswith('.epub') or 'META-INF/container.xml' in z.namelist():
                names = epub_pictures(z)
            else:
                names = sorted((n for n in z.namelist() if is_picture(n)), key=natural_key)
            for name in names:
                with Image.open(io.BytesIO(z.read(name))) as im:
                    yield name, im.convert('RGB')
    else:
        with Image.open(source) as im:
            yield os.path.basename(source), im.convert('RGB')


# ---------------------------------------------------------------- page analysis

def paper_colour(page):
    """The most common colour along the page's edges."""
    small = page.resize((max(1, page.width // 8), max(1, page.height // 8)), Image.BOX)
    w, h = small.size
    edge = [small.getpixel((x, 0)) for x in range(w)] + [small.getpixel((x, h - 1)) for x in range(w)]
    edge += [small.getpixel((0, y)) for y in range(h)] + [small.getpixel((w - 1, y)) for y in range(h)]
    # Near colours count together (scan noise).
    buckets = {}
    for c in edge:
        key = (c[0] // 16, c[1] // 16, c[2] // 16)
        buckets.setdefault(key, []).append(c)
    group = max(buckets.values(), key=len)
    return tuple(sum(c[k] for c in group) // len(group) for k in range(3))


def ink_mask(picture, paper, threshold=40):
    """255 where the picture clearly differs from the paper."""
    diff = ImageChops.difference(picture, Image.new('RGB', picture.size, paper)).convert('L')
    return diff.point(lambda v: 255 if v > threshold else 0)


def profiles(mask):
    """Ink per row and per column of a mask."""
    w, h = mask.size
    rows = [count for count in (mask.crop((0, y, w, y + 1)).histogram()[255] for y in range(h))]
    columns = [count for count in (mask.crop((x, 0, x + 1, h)).histogram()[255] for x in range(w))]
    return rows, columns


def crop_box(page, paper, mode):
    """The part of the page read: all of it, its ink (light), or its main ink (strong)."""
    if mode == 'off':
        return (0, 0, page.width, page.height)
    scale = 4
    small = page.resize((max(1, page.width // scale), max(1, page.height // scale)), Image.BOX)
    rows, columns = profiles(ink_mask(small, paper))
    w, h = small.size

    def bounds(ink, count, threshold, margin):
        low = 0
        while low < count and ink[low] < threshold: low += 1
        high = count
        while high > low and ink[high - 1] < threshold: high -= 1
        if low >= high: return 0, count
        return max(0, low - margin), min(count, high + margin)

    if mode == 'light':
        x0, x1 = bounds(columns, w, 1, w // 50 + 1)
        y0, y1 = bounds(rows, h, 1, h // 50 + 1)
    else:
        x0, x1 = bounds(columns, w, max(2, h * 3 // 100), w // 100 + 1)
        y0, y1 = bounds(rows, h, max(2, w * 3 // 100), h // 100 + 1)
    # Much smaller than the page: more likely a mistake.
    if (x1 - x0) * 10 < w * 3 or (y1 - y0) * 10 < h * 3:
        return (0, 0, page.width, page.height)
    return (x0 * scale, y0 * scale, min(page.width, x1 * scale), min(page.height, y1 * scale))


# ---------------------------------------------------------------- pictures and screens

def make_picture(source, w, h, options):
    """Scaled straight from the original, sharpened, in COLORS colours."""
    if options.boost_lettering:
        # Dark strokes one pixel wider before shrinking: small lettering stays dark.
        source = source.filter(ImageFilter.MinFilter(3))
    im = source.resize((w, h), Image.LANCZOS, reducing_gap=3.0)
    sharpen = {'off': 0, 'light': 70, 'strong': 150}[options.sharpen]
    if sharpen:
        im = im.filter(ImageFilter.UnsharpMask(radius=1.0, percent=sharpen, threshold=2))
    q = im.quantize(COLORS, method=Image.Quantize.MEDIANCUT)
    if options.dither:
        q = im.quantize(palette=q, dither=Image.Dither.FLOYDSTEINBERG)
    return q


def fit(w0, h0, cw, ch):
    if h0 * cw <= ch * w0:
        return cw, max(1, round(h0 * cw / w0))
    return max(1, round(w0 * ch / h0)), ch


class Picture:
    """One picture of a view: pixels (screen colour indices), palette, where
    on the page it is, and which 8 x 8 cells have ink (the GBA chooses the
    screens itself)."""

    def __init__(self, q, box, page_size, cw, ink):
        self.w, self.h = q.size
        self.pixels = bytes(v + FIRST for v in q.tobytes())
        pal = q.getpalette()[:COLORS * 3]
        pal += [255] * (COLORS * 3 - len(pal))
        self.palette = [(pal[3 * i] >> 3) | ((pal[3 * i + 1] >> 3) << 5) | ((pal[3 * i + 2] >> 3) << 10)
                        for i in range(COLORS)]
        pw, ph = page_size
        self.box = (box[0] * PANEL_UNIT // pw, box[1] * PANEL_UNIT // ph,
                    -(-box[2] * PANEL_UNIT // pw), -(-box[3] * PANEL_UNIT // ph))
        self.strip_width = cw // 2
        self.grid_w, self.grid_h = -(-self.w // CELL), -(-self.h // CELL)
        # A cell has ink when any of its pixels does (the cell's average is not 0).
        padded = Image.new('L', (self.grid_w * CELL, self.grid_h * CELL), 0)
        padded.paste(ink, (0, 0))
        cells = padded.resize((self.grid_w, self.grid_h), Image.BOX).tobytes()
        bits = bytearray(-(-(self.grid_w * self.grid_h) // 8))
        for i, v in enumerate(cells):
            if v:
                bits[i >> 3] |= 1 << (i & 7)
        self.ink = bytes(bits)


def picture_for(source, box, page_size, w, h, cw, options, paper):
    """The picture of `box` of the page at w x h."""
    q = make_picture(source.crop(box), w, h, options)
    return Picture(q, box, page_size, cw, ink_mask(q.convert('RGB'), paper))


def views_for_page(page, options):
    """(zoom index, pictures, orders) for every view of one page."""
    paper = paper_colour(page)
    box = crop_box(page, paper, options.margins)
    bw, bh = box[2] - box[0], box[3] - box[1]
    page_size = page.size
    views = []
    for index, (orientation, kind, factor) in enumerate(options.zoom_list):
        cw, ch = ORIENTATIONS[orientation]
        pictures = []
        if kind == KIND_FIT_PAGE:
            spread = {'auto': bw * 10 > bh * 11, 'always': bw * 10 > bh * 8, 'never': False}[options.spreads]
            parts = [box] if not spread else [
                (box[0], box[1], box[0] + bw // 2, box[3]), (box[0] + bw // 2, box[1], box[2], box[3])]
            for part in parts:
                w, h = fit(part[2] - part[0], part[3] - part[1], cw, ch)
                pictures.append(picture_for(page, part, page_size, w, h, cw, options, paper))
            orders = [list(range(len(parts))), list(range(len(parts)))[::-1], list(range(len(parts)))]
        else:
            w = round(cw * factor / 100)
            h = max(1, round(bh * w / bw))
            pictures.append(picture_for(page, box, page_size, w, h, cw, options, paper))
            orders = [[0], [0], [0]]
        views.append((index, pictures, orders))
    return views


def zoom_label(orientation, kind, factor):
    angle = 'upright ' if orientation == 0 else 'sideways'
    if kind == KIND_FIT_PAGE:
        return angle + ' fit page '
    return angle + (' fit width' if factor == 100 else f' {factor / 100:g}x').ljust(10)


# ---------------------------------------------------------------- compression and file

def lz77(data):
    """GBA BIOS LZ77 (type 0x10): matches of 3..18 bytes up to 4096 back."""
    n = len(data)
    out = bytearray((0x10 | (n << 8)).to_bytes(4, 'little'))
    table = {}
    i = 0
    while i < n:
        flag_at = len(out)
        out.append(0)
        flags = 0
        for bit in range(8):
            if i >= n:
                break
            best_len = best_d = 0
            if i + 3 <= n:
                cands = table.get(data[i:i + 3])
                if cands:
                    limit = min(18, n - i)
                    for c in reversed(cands):
                        d = i - c
                        if d > 4096:
                            break
                        length = 3
                        while length < limit and data[c + length] == data[i + length]:
                            length += 1
                        if length > best_len:
                            best_len, best_d = length, d
                            if length == limit:
                                break
            step = best_len if best_len >= 3 else 1
            if best_len >= 3:
                flags |= 0x80 >> bit
                out.append(((best_len - 3) << 4) | ((best_d - 1) >> 8))
                out.append((best_d - 1) & 0xFF)
            else:
                out.append(data[i])
            for p in range(i, i + step):
                if p + 3 <= n:
                    key = data[p:p + 3]
                    lst = table.get(key)
                    if lst is None:
                        table[key] = [p]
                    else:
                        lst.append(p)
                        if len(lst) > 24:
                            del lst[:8]
            i += step
        out[flag_at] = flags
    while len(out) & 3:
        out.append(0)
    return bytes(out)


def unlz77(data):
    n = data[1] | data[2] << 8 | data[3] << 16
    out = bytearray()
    i = 4
    while len(out) < n:
        flags = data[i]
        i += 1
        for bit in range(8):
            if len(out) >= n:
                break
            if flags & (0x80 >> bit):
                length = (data[i] >> 4) + 3
                d = ((data[i] & 15) << 8 | data[i + 1]) + 1
                i += 2
                for _ in range(length):
                    out.append(out[-d])
            else:
                out.append(data[i])
                i += 1
    return bytes(out)


class Writer:
    def __init__(self, f):
        self.f = f

    def at(self):
        return self.f.tell()

    def put(self, data):
        self.f.write(data)

    def align(self):
        while self.f.tell() & 3:
            self.f.write(b'\0')

    def patch(self, at, data):
        here = self.f.tell()
        self.f.seek(at)
        self.f.write(data)
        self.f.seek(here)


def write_picture(out, p, options):
    """Picture: header, ink grid, palette, chunk index, chunks (see the format doc)."""
    out.align()
    start = out.at()
    strip_count = -(-p.w // p.strip_width)
    chunks = -(-p.h // CHUNK_ROWS)
    out.put(struct.pack('<HHHHHH4HHH', p.w, p.h, p.strip_width, strip_count, p.grid_w, p.grid_h,
                        *p.box, CHUNK_ROWS, 0))
    out.put(p.ink)
    out.align()
    out.put(struct.pack('<%dH' % COLORS, *p.palette))
    index_at = out.at()
    out.put(b'\0' * 4 * (strip_count * chunks + 1))
    offsets = []
    packed_total = 0
    for s in range(strip_count):
        x0 = s * p.strip_width
        x1 = min(p.w, x0 + p.strip_width)
        for k in range(chunks):
            y0 = k * CHUNK_ROWS
            y1 = min(p.h, y0 + CHUNK_ROWS)
            raw = b''.join(p.pixels[y * p.w + x0:y * p.w + x1] for y in range(y0, y1))
            packed = lz77(raw)
            if options.verify and unlz77(packed) != raw:
                sys.exit('internal error: compression check failed')
            offsets.append(out.at())
            out.put(packed)
            packed_total += len(packed)
    offsets.append(out.at())
    out.patch(index_at, struct.pack('<%dI' % len(offsets), *offsets))
    return start, packed_total


def convert(options, output):
    pages = load_pages(options.source, options.dpi)
    tmp = output + '.part'
    zooms = options.zoom_list
    header_size = 32 + 4 * len(zooms)
    stats = [0] * len(zooms)
    count = 0
    with open(tmp, 'wb') as f:
        out = Writer(f)
        out.put(b'\0' * header_size)             # header, written last
        page_table = []
        for name, page in pages:
            if count == 65535:
                sys.exit('Too many pages (65535 max).')
            views = views_for_page(page, options)
            out.align()
            record = out.at()
            page_table.append(record)
            out.put(struct.pack('<HHHH', len(views), 0, *[min(65535, v) for v in page.size]))
            entries_at = out.at()
            out.put(b'\0' * 8 * len(views))
            entries = []
            for index, pictures, orders in views:
                out.align()
                view_at = out.at()
                for order in orders:
                    out.put(bytes(order))
                out.align()
                table_at = out.at()
                out.put(b'\0' * 4 * len(pictures))
                offsets = []
                for p in pictures:
                    at, packed = write_picture(out, p, options)
                    offsets.append(at)
                    stats[index] += packed
                out.patch(table_at, struct.pack('<%dI' % len(offsets), *offsets))
                entries.append(struct.pack('<BBHI', index, 0, len(pictures), view_at))
            out.patch(entries_at, b''.join(entries))
            count += 1
            progress = getattr(options, 'progress', None)
            if progress:
                progress(count, name)     # may raise to stop
            else:
                print(f'page {count}: {name}', file=sys.stderr, flush=True)
        if not count:
            sys.exit('No pictures found.')
        out.align()
        table = out.at()
        page_table.append(table)
        out.put(struct.pack('<%dI' % len(page_table), *page_table))
        size = out.at()
        orientations = 0
        for orientation, _, _ in zooms:
            orientations |= 1 << orientation
        out.patch(0, b'GBCOMIC1' + struct.pack('<HHBBHII', VERSION, count, orientations, len(zooms), header_size,
                                               table, size) + b'\0' * 8
                  + b''.join(struct.pack('<BBH', *z) for z in zooms))
    os.replace(tmp, output)
    return count, stats


def factors(text, choices, name):
    """'1.6,1.8,2' -> [160, 180, 200]: zooms in hundredths, smallest first,
    without 1.0 (Fit width, always there). Raises ValueError (with the
    reason) for anything not in choices."""
    result = set()
    for part in text.replace(' ', ',').split(','):
        part = part.strip().lower().rstrip('x')
        if not part:
            continue
        try:
            value = round(float(part) * 100)
        except ValueError:
            raise ValueError(f'{name}: not a number: {part}')
        if value not in choices and value != 100:
            raise ValueError(f'{name}: {part} is not one of ' + ' '.join(zoom_text(c) for c in choices))
        if value != 100:
            result.add(value)
    return sorted(result)


def zoom_text(value):
    return f'{value / 100:.1f}'


# Rough size per page of a colour comic at 200 dpi (measured on a 25-page comic).
def estimate_kb(orientation, values):
    if orientation == 0:
        return 15 + 68 + sum(57.5 * (v / 100) ** 2 for v in values)
    return 32 + 33 + sum(28.3 * (v / 100) ** 2 for v in values)


def ask(options):
    """Asks for the angles and zooms (the Windows and Linux packages start with this)."""
    def prompt(text):
        try:
            return input(text).strip()
        except EOFError:
            return ''
    print('gbcomic converter: PDF, EPUB, CBZ, picture folders -> .gbcomic for gbareader')
    print()
    print('Screen angles:  1 = both (default)   2 = upright only (Normal, Upside down)   3 = sideways only (90°)')
    while True:
        answer = prompt('Angles [1]: ') or '1'
        if answer in ('1', '2', '3'):
            options.angles = {'1': 'both', '2': 'upright', '3': 'sideways'}[answer]
            options.orientations = {'both': [0, 1], 'upright': [0], 'sideways': [1]}[options.angles]
            break
        print('  Please type 1, 2 or 3.')
    lists = {}
    for orientation, name, choices, default in ((0, 'Upright zooms', NORMAL_CHOICES, DEFAULT_NORMAL),
                                                (1, 'Sideways (90°) zooms', SIDEWAYS_CHOICES, DEFAULT_SIDEWAYS)):
        if orientation not in options.orientations:
            continue
        print()
        print(f'{name}: ' + ' '.join(zoom_text(c) for c in choices))
        print('  Any of them, separated by commas or spaces; Fit page and Fit width (1.0) are always included.')
        while True:
            answer = prompt(f'  [{default}]: ') or default
            try:
                lists[orientation] = factors(answer, choices, name)
                break
            except ValueError as e:
                print('  ' + str(e))
    options.normal = ','.join(zoom_text(v) for v in lists.get(0, [])) or DEFAULT_NORMAL
    options.sideways = ','.join(zoom_text(v) for v in lists.get(1, [])) or DEFAULT_SIDEWAYS
    kb = sum(estimate_kb(o, lists[o]) for o in lists)
    print()
    print(f'About {kb / 1024:.1f} MB per page for a colour comic (black and white: less).')
    print()


def main():
    parser = argparse.ArgumentParser(
        description='Convert comics (PDF, EPUB, CBZ, a folder of pictures) to .gbcomic files for gbareader.')
    parser.add_argument('sources', nargs='*', metavar='source')
    parser.add_argument('-o', '--output', help='output file (one source only; default: next to the source, .gbcomic)')
    parser.add_argument('--ask', action='store_true', help='ask for the angles and zooms first')
    parser.add_argument('--angles', choices=['both', 'upright', 'sideways'], default='both',
                        help='screens for the upright angles (Normal, Upside down), the sideways ones (90°), or both')
    parser.add_argument('--normal', default=DEFAULT_NORMAL,
                        help='zooms for the upright angles, any of %s (default %%(default)s); '
                             'Fit page and Fit width are always there'
                             % ' '.join(zoom_text(c) for c in NORMAL_CHOICES))
    parser.add_argument('--sideways', default=DEFAULT_SIDEWAYS,
                        help='zooms for the sideways angles, any of %s (default %%(default)s)'
                             % ' '.join(zoom_text(c) for c in SIDEWAYS_CHOICES))
    parser.add_argument('--dither', action='store_true', help='dither colours (smoother gradients, a little larger)')
    parser.add_argument('--sharpen', choices=['off', 'light', 'strong'], default='light')
    parser.add_argument('--boost-lettering', action='store_true', help='keep small lettering dark')
    parser.add_argument('--margins', choices=['off', 'light', 'strong'], default='light',
                        help='crop blank paper (light) and page numbers / scan edges (strong)')
    parser.add_argument('--spreads', choices=['auto', 'always', 'never'], default='auto',
                        help='wide pages in two halves at Fit page')
    parser.add_argument('--dpi', type=int, default=200, help='resolution for PDF pages')
    parser.add_argument('--verify', action='store_true', help='check every compressed chunk')
    options = parser.parse_args()
    # The ready-made programs (files dropped on them, or started alone) ask first.
    if getattr(sys, 'frozen', False) and not any(a.startswith('-') for a in sys.argv[1:]):
        options.ask = True
    sources = list(options.sources)
    if options.ask and not sources:
        print('Drop comic files on the gbcomic program, or type (or drag in) one here.')
        try:
            typed = input('File: ').strip().strip('"').strip("'")
        except EOFError:
            typed = ''
        if typed:
            sources = [typed]
    if not sources:
        parser.error('no comic given')
    if options.output and len(sources) > 1:
        parser.error('-o needs a single source')
    for source in sources:
        if not os.path.exists(source):
            parser.error(f'not found: {source}')
    options.orientations = {'both': [0, 1], 'upright': [0], 'sideways': [1]}[options.angles]
    if options.ask and sys.stdin.isatty():
        ask(options)
    try:
        lists = {0: factors(options.normal, NORMAL_CHOICES, '--normal'),
                 1: factors(options.sideways, SIDEWAYS_CHOICES, '--sideways')}
    except ValueError as e:
        parser.error(str(e))
    options.zoom_list = zoom_list(options.angles, lists[0], lists[1])
    failed = 0
    for source in sources:
        options.source = source
        output = options.output or os.path.splitext(source.rstrip('/\\'))[0] + '.gbcomic'
        try:
            count, stats = convert(options, output)
        except (SystemExit, Exception) as e:
            print(f'{source}: not converted: {e}', file=sys.stderr)
            if os.path.exists(output + '.part'):
                os.remove(output + '.part')
            failed += 1
            continue
        total = os.path.getsize(output)
        print(f'{output}: {count} pages, {total / 2**20:.1f} MB ({total / count / 1024:.0f} KB per page)')
        for zoom, packed in zip(options.zoom_list, stats):
            print(f'  {zoom_label(*zoom)}  {packed / count / 1024:5.0f} KB/page')
    if failed:
        sys.exit(1)


if __name__ == '__main__':
    main()
