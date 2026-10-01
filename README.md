# gbareader

**Read books on your Game Boy Advance.**

gbareader turns a Game Boy Advance with a Supercard SD into a pocket e-reader. Put your TXT and EPUB books on the SD card, pick one from the list, and start reading. Press Start to save your page, and the next time you open the book you are right back where you left off.

## Get gbareader

**The ready-to-play ROM is available on itch.io: [halimj.itch.io](https://halimj.itch.io/)**

That is the easiest way to get gbareader, and it supports further development.

This repository contains the complete source code, free and open under the GPL v3.

## What it can do

- **Reads TXT and EPUB books** straight from the SD card, no conversion needed.
- **Remembers your page** for every book, plus your reading settings and the last book you opened.
- **Handles big books and big libraries**: books are read from the card a piece at a time, and there is no limit on how many books you keep.
- **Imports books for you**: browse the whole SD card and copy a book into your library with a button press.
- **Lets you tune the page**: line spacing, space between paragraphs, page numbers, a "Go to" percentage to jump anywhere in a book, and file extensions shown or hidden in the book list.
- **Shows many languages**: Latin, Greek, Cyrillic, Japanese, Chinese and Korean text, plus Arabic with properly joined letters written right to left.
- **Never changes your books**: your files are only read. Your progress is kept in a small `.sav` file next to each book.

## Getting started

1. Copy `gbareader.gba` to your Supercard SD card.
2. Create a folder called `gbareader` at the top of the card and put your `.txt` and `.epub` books in it. (You can also skip this and import books from inside the app.)
3. Start gbareader on your Game Boy Advance and choose a book.

Text files should be saved as UTF-8. EPUBs are shown as text only: pictures, special layouts and copy-protected (DRM) books are not supported.

## Controls

**Book list**
- **Up / Down**: choose a book. **Left / Right**: jump a page of books.
- **A**: open the book.
- **Start**: import a book from anywhere on the SD card.
- **Select**: settings.

**While reading**
- **Right or A**: next page. **Left or B**: previous page.
- **Start**: save your place.
- **Select**: settings, including *Back to Files* to return to the book list.
- **Hold Up**: turn pages with the shoulder buttons L and R instead.

All controls are explained in the [full controls guide (PDF)](docs/gbareader-full-controls.pdf), also available [as text](docs/gbareader-full-controls.md).

## Good to know

- **Keep each book and its `.sav` file together.** If you rename or move a book, rename or move its `.sav` file the same way, or the book starts from the beginning.
- **Don't switch off or remove the card while it is saving.** Keeping a backup of your SD card is always a good idea.
- **Arabic** is switched on automatically when a book opens on a page with Arabic text. More in [Arabic support](docs/arabic.md).
- gbareader is checked with automated tests and on an emulator. Testing on real Supercard hardware is still limited.

For the technical details of how saves work, see [the save file format](docs/sidecar-format.md).

## Building it yourself

You need [devkitPro](https://devkitpro.org/) with devkitARM, GNU Make, Python 3 and the [Butano](https://github.com/GValiente/butano) engine (commit `77dcbcb3d8783596a9f333c64eedbccec77b05dc`).

```sh
make -j2 LIBBUTANO=/absolute/path/to/butano/butano
```

This creates `gbareader.gba`. The build is reproducible: the same source with the same tools gives exactly the same ROM, wherever you build it. The easiest way to get the same tools is the `devkitpro/devkitarm:20260610` Docker image.

To run the tests:

```sh
bash tests/run_host_tests.sh
python3 tests/fatfs/run.py
```

The SD card image tests also need GCC, `dosfstools` and `mtools`; see the [test notes](tests/fatfs/README.md).

## Credits and license

Made by **Halim Jarrar** · © 2026 · [halimj.itch.io](https://halimj.itch.io/) · gba@halim-jarrar.de

gbareader is free software under the [GNU GPL v3 or later](LICENSE). It builds on the work of others:

- The Supercard SD and FatFs support comes from the author's earlier Game Boy Advance vocabulary trainer.
- [SuperFW](references/superfw/) font rendering and font packs (GPL v3 or later). The UNSCII-based pack includes GNU Unifont and public-domain Fixedsys Excelsior glyphs.
- [Ghoulam](https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1) Arabic pixel font © 2025 Imad AlFil / mloukhiyye, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), converted to fit the Game Boy screen. This conversion is not endorsed by the font's author.
- [Butano](https://github.com/GValiente/butano) game engine (zlib license).
- FatFs by ChaN (permissive license).
- The decompression part of [miniz](third_party/miniz/) (MIT license).

The credits are also shown in the app under Settings › About.
