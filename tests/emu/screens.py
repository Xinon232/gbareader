#!/usr/bin/env python3
"""Exact-ROM UI screenshots on mGBA with the modeled Supercard SD card.

Builds ui_runner (needs libmgba-dev), makes a FAT16 card with books and
folders (needs mtools, dosfstools), drives the ROM with scripted buttons and
writes contact sheets (needs Pillow) to OUTDIR. Not physical hardware proof.
Usage: tests/emu/screens.py gbareader.gba OUTDIR
"""
import os, subprocess, sys, tempfile
from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
K = dict(A=1, B=2, SEL=4, START=8, RIGHT=16, LEFT=32, UP=64, DOWN=128, R=256, L=512)


def make_card(work, books):
    img = work / 'card.img'
    subprocess.run(['truncate', '-s', '64M', str(img)], check=True)
    subprocess.run(['mkfs.vfat', '-F', '16', str(img)], check=True, capture_output=True)
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    text = work / 'book.txt'
    words = 'the quick brown fox jumps over a lazy dog while reading at night'.split()
    text.write_text('\n\n'.join(' '.join(words[(i * 7 + j) % len(words)] for j in range(60)).capitalize() + '.'
                                for i in range(300)))
    def run(*a): subprocess.run(list(a), check=True, env=env)
    run('mmd', '-i', str(img), '::/Downloads', '::/Downloads/Novels')
    if books:
        run('mmd', '-i', str(img), '::/gbareader')
        run('mcopy', '-i', str(img), str(text), '::/gbareader/Alice.txt')
        run('mcopy', '-i', str(img), str(text), '::/gbareader/A Tale of Two Cities - Charles Dickens - Complete Edition.txt')
        for i in range(1, 10):
            run('mcopy', '-i', str(img), str(text), f'::/gbareader/Short story {i}.txt')
    run('mcopy', '-i', str(img), str(text), '::/Downloads/Pride and Prejudice.txt')
    run('mcopy', '-i', str(img), str(text), '::/Downloads/notes.pdf')
    run('mcopy', '-i', str(img), str(text), '::/Downloads/Novels/War and Peace.txt')
    return img


def tour(runner, rom, card, steps, out):
    f, keys, shots = 300, [], {}
    for st in steps:
        if st[0] == 'shot': shots[f] = st[1]; continue
        if st[0] == 'wait': f += st[1]; continue
        hold = st[1] if len(st) > 1 else 4
        keys.append((f, K[st[0]], hold)); f += hold + 12
    out.mkdir(parents=True, exist_ok=True)
    (out / 'keys.txt').write_text(''.join(f'{a} {b} {c}\n' for a, b, c in keys))
    env = dict(os.environ, SD_IMAGE=str(card), SD_ALLOW_INDEX_WRITES='1',
               KEYS_FILE=str(out / 'keys.txt'), SHOTS=','.join(map(str, shots)))
    subprocess.run([str(runner), str(rom), str(f + 1), str(out)], env=env, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return [(label, Image.open(out / f'frame-{fr:05d}.ppm').convert('RGB')) for fr, label in sorted(shots.items())]


def sheet(items, path, cols=3):
    W, H, pad, lab = 480, 320, 8, 18
    rows = (len(items) + cols - 1) // cols
    im = Image.new('RGB', (cols * (W + pad) + pad, rows * (H + lab + pad) + pad), (60, 60, 60))
    d = ImageDraw.Draw(im)
    for i, (label, shot) in enumerate(items):
        x, y = pad + (i % cols) * (W + pad), pad + (i // cols) * (H + lab + pad)
        d.text((x, y), label, fill=(255, 255, 255))
        im.paste(shot.resize((W, H), Image.NEAREST), (x, y + lab))
    im.save(path)


def main():
    rom, outdir = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    outdir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        runner = work / 'ui_runner'
        subprocess.run(['cc', '-O2', str(HERE / 'ui_runner.c'), '-lmgba', '-o', str(runner)], check=True)
        card = make_card(work, True)
        sheet(tour(runner, rom, card, [
            ('shot', 'Home'), ('DOWN',), ('wait', 150), ('shot', 'Long name scrolls'),
            ('SEL',), ('shot', 'Select: Settings'), ('UP',), ('A',), ('shot', 'About 1/7'),
            *[s for i in range(2, 8) for s in (('RIGHT',), ('shot', f'About {i}/7'))], ('B',), ('B',),
            ('START',), ('shot', 'Start: Import'), ('A',), ('shot', '/Downloads'), ('DOWN',), ('A',),
            ('shot', 'Confirm'), ('DOWN',), ('A',), ('wait', 60), ('shot', 'Imported'),
            ('A',), ('wait', 40), ('shot', 'Reading'), ('UP', 60), ('shot', 'Hold Up: L+R'), ('wait', 60),
            ('SEL',), ('wait', 30), ('shot', 'Settings while reading'),
        ], work / 'a'), outdir / 'screens.png')
        empty = make_card(work, False)
        sheet(tour(runner, rom, empty, [('shot', 'No books'), ('START',), ('A',), ('A',), ('DOWN',), ('A',),
                                         ('wait', 60), ('shot', 'First import creates /gbareader')],
                   work / 'b'), outdir / 'screens-empty.png')
    print('Wrote', outdir / 'screens.png', 'and', outdir / 'screens-empty.png',
          '(modeled SD card, not physical hardware)')


if __name__ == '__main__':
    main()
