# gbareader 10.0

**Read books and comics on your Game Boy Advance.**

gbareader turns a Game Boy Advance with a flashcart (SuperCard SD, EZ-Flash Omega or EverDrive GBA) into a pocket e-reader. Put your TXT and EPUB books and your comics on the SD card, pick one from the list, and start reading. Press Start to save your page, and the next time you open the book you are right back where you left off.

This repository holds the complete source code of gbareader 10.0: the ROM, the gbcomic conversion software for comics (`tools/gbcomic*.py`, `tools/converter/`) and the instructions (`docs/gbareader-guide.pdf`, `docs/gbareader-full-controls.pdf`). It contains no ready-made ROM or programs. The ROM and the ready-made conversion software (Windows and Linux) are free at **[halimj.itch.io](https://halimj.itch.io/)**, or [build it yourself](#building-it-yourself).

## What it can do

- **Reads TXT and EPUB books** straight from the SD card, no conversion needed.
- **Reads comics** as `.gbcomic` files made with the gbcomic conversion software from PDF, EPUB, CBZ or a folder of pictures. Zoom in, read screen by screen with A and B, move around the page with the D-pad, and hold R for a closer look (or L to see more of the page).
- **Turns the screen**: read upright or hold the GBA sideways (90°) or upside down. Each book keeps its own angle and comic view.
- **Remembers your page** for every book, plus your reading settings and the last book you opened.
- **Handles big books and big libraries**: books are read from the card a piece at a time, and there is no limit on how many books you keep.
- **Imports books for you**: browse the whole SD card and copy a book into your library with a button press.
- **Shows many languages**: Latin, Greek, Cyrillic, Japanese, Chinese and Korean text, plus Arabic with properly joined letters written right to left.
- **Never changes your books**: your files are only read. Your progress is kept in a small `.sav` file next to each book.

## Getting started

1. Copy `gbareader.gba` to your SD card.
2. Create a folder called `gbareader` at the top of the card and put your `.txt`, `.epub` and `.gbcomic` files in it. (You can also skip this and import books from inside the app.)
3. Start gbareader on your Game Boy Advance and choose a book.

The SD card can be formatted FAT16, FAT32 or exFAT. One ROM runs on the SuperCard SD, the EZ-Flash Omega (also the Definitive Edition; start the ROM from the SD card, not from NOR) and the EverDrive GBA; hold **L+R** while it starts to use the SuperCard driver. All controls are in the [instructions](docs/gbareader-guide.pdf) and the [full controls](docs/gbareader-full-controls.md) ([PDF](docs/gbareader-full-controls.pdf)).

## Building it yourself

You need:

- [devkitPro](https://devkitpro.org/) with devkitARM (the release ROM was built with devkitARM r68, GCC 16.1.0; the `devkitpro/devkitarm:20260610` Docker image has the same tools), GNU Make and Python 3;
- the [Butano](https://github.com/GValiente/butano) engine at commit `77dcbcb3d8783596a9f333c64eedbccec77b05dc`;
- [gbaSD](https://github.com/Xinon232/gbaSD), the SD card code, a git submodule of this repository (`gbasd`, pinned to the commit the release was built with).

```sh
git clone --recursive https://github.com/Xinon232/gbareader.git
# (in a clone made without --recursive: git submodule update --init)
git clone https://github.com/GValiente/butano.git
git -C butano checkout 77dcbcb3d8783596a9f333c64eedbccec77b05dc
cd gbareader
make -j2 LIBBUTANO="$PWD/../butano/butano"
```

With the Docker image instead of a local devkitPro, build from the folder that holds `gbareader` and `butano`:

```sh
docker run --rm -v "$PWD:/work" -w /work/gbareader devkitpro/devkitarm:20260610 \
    make -j2 LIBBUTANO=/work/butano/butano
```

This creates `gbareader.gba` (and `gbareader.elf`). The build is reproducible: the same source with the same tools gives exactly the same ROM in any folder, so your `gbareader.gba` has the same SHA-256 as the released one.

The conversion software runs from source with Python 3.8 or newer, Pillow and, for PDF, pypdfium2: `python3 tools/gbcomic.py --help` (command line) or `python3 tools/gbcomic_gui.py` (the window; needs Tkinter). `tools/converter/build.sh` builds the ready-made Windows and Linux programs; see [tools/converter/README.md](tools/converter/README.md).

## License

gbareader 10.0 · © 2026 Halim Jarrar · [halimj.itch.io](https://halimj.itch.io/) · gba@halim-jarrar.de

gbareader is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version (GPL-3.0-or-later, see [LICENSE](LICENSE)).

gbareader is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## Credits

gbareader builds on the work of others (each keeps its own license and notices in its files):

- **SuperFW** by David Guillen Fandos, GPL v3 or later: the font renderer and font packs (`references/superfw/`); the UNSCII-based pack includes GNU Unifont and public-domain Fixedsys Excelsior glyphs.
- **UNSCII** by Viznut, GPL; **GNU Unifont** (also its Hangul) by Roman Czyborra, Paul Hardy and Unifont contributors, GPL v2 or later with the GNU font embedding exception.
- **Ghoulam** Arabic pixel font © 2025 Imad AlFil / mloukhiyye, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) ([source](https://mloukhiyye.itch.io/ghoulam-arabic-pixel-art-font-version-1)), converted to an 11-pixel bitmap font for the Game Boy screen (`include/ghoulam_data.h`, made by `tools/extract_ghoulam.py`). This conversion is not endorsed by the font's author.
- **SD card drivers** from [gbaSD](https://github.com/Xinon232/gbaSD): the SuperCard SD driver from SuperFW by David Guillen Fandos (GPL v3 or later), the EverDrive GBA driver ported from edio-gba by krikzz (MIT license) and the EZ-Flash Omega driver by Halim Jarrar (GPL v3 or later), written after the EZ-Flash Omega kernels (Apache 2.0). See `gbasd/CREDITS.md`.
- **FatFs** by ChaN (FatFs license, a permissive license), from gbaSD (`gbasd/fatfs`).
- **Butano** game engine by Gustavo Valiente (zlib license, `third_party/butano/LICENSE`); the common font headers in `include/common_*` come from it.
- The decompression part of **miniz** (MIT license, `third_party/miniz/`): © 2010–2014 Rich Geldreich and Tenacious Software LLC, © 2013–2014 RAD Game Tools and Valve Software.
- The 5x7 menu font from [gbamp3](https://github.com/Xinon232/gbamp3) by Halim Jarrar (GPL v3 or later).
- Built with devkitARM / devkitPro.

The credits are also shown in the app under Settings › About and on the last page of both PDFs. Technical details: [technical notes](docs/technical-notes.md), [the save file format](docs/sidecar-format.md), [the .gbcomic file format](docs/gbcomic-format.md), [Arabic support](docs/arabic.md); changes: [release notes](RELEASE_NOTES.md).
