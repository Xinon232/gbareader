# GBA Vocab Trainer

A simple 5-box vocabulary trainer for the Game Boy Advance, built with Butano and targeted at SuperFW / Supercard SD-style setups.

The trainer can load vocabulary from `.txt` files compatible with dict.cc-style vocab-trainer exports. Put text files on the SD card and select them from the in-game file browser.

Current text support:

- Latin letters, including German umlauts such as ä, ö, ü, Ä, Ö, Ü and ß
- Russian Cyrillic letters, using Cyrillic glyphs extracted from SuperFW's own font pack for a matching visual style

Arabic work is experimental and not considered supported yet.

## Controls

Training screen:

- R: hold to reveal the answer
- A: mark the current word correct and move it to the next box
- B: reset the current word back to box 1
- D-pad Left / Right: switch between boxes 1-5
- D-pad Up: undo the most recent A/B decision, if you stayed in the same box
- L: cycle direction mode: front-to-back, back-to-front, alternating
- Start: save/export the current progress
- Select: open the file browser

File browser:

- D-pad Up / Down: move through files
- D-pad Left / Right: jump by 5 files
- A: load selected `.txt` file
- B: return to training

## File format

The project is intended for dict.cc-style tab-separated vocabulary text files, for example:

```text
Haus	house
Hund	dog
дом	house
```

The importer keeps 5-box progress when reopening files saved/exported by the trainer.

## Build

Requirements:

- devkitPro / devkitARM
- Butano
- Python 3

Build:

```bash
make LIBBUTANO=/path/to/butano/butano
```

The ROM output is `vocab.gba`.

## Notes

This is an early public source snapshot. It is useful for experimentation and for testing dict.cc vocabulary files on GBA hardware, but it is not a polished release yet.
