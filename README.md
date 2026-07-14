# GBA Vocab Trainer

A simple 5-box vocabulary trainer for the Game Boy Advance, built with Butano and targeted at SuperFW / Supercard SD-style setups.

The trainer can load vocabulary from `.txt` files compatible with dict.cc-style vocab-trainer exports. Put text files on the SD card and select them from the in-game file browser.

Each vocabulary file can contain up to 10,000 entries. The text is streamed from the SD card, so smaller files retain their normal per-file loading, saving, and training performance.

Current flashcard text support uses SuperFW-derived fonts for broad language compatibility:

- Latin Extended (`U+0080–U+024F`) for Western/Central European languages and phonetic/diacritic-heavy entries
- Greek and Cyrillic (`U+0370–U+04FF`) including Russian, Ukrainian, Bulgarian, Serbian/Macedonian-style Cyrillic extensions, and Greek
- Japanese punctuation, Hiragana, and Katakana (`U+3000–U+30FF`)
- CJK Unified Ideographs (`U+4E00–U+9FEF`) plus SuperFW's included CJK Extension-B subset (`U+20000–U+200CC`) for Chinese/Japanese/Korean Han characters
- Korean Hangul syllables (`U+AC00–U+D7A3`)
- Arabic is still rendered with the existing experimental Arabic font path and is not considered fully supported yet because shaping/joining is incomplete

## Controls

Training screen:

- R: hold to reveal the answer
- A: mark the current word correct and move it to the next box; shows a green feedback flash with word + answer before advancing
- B: reset the current word back to box 1; shows a red feedback flash with word + answer before advancing
- D-pad Left / Right: switch between boxes 1-5
- D-pad Up: undo the most recent A/B decision, if you stayed in the same box
- D-pad Down: ask to shuffle only the current box
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

If no SD-card vocabulary file is loaded yet, the built-in starter list shows one language-name sample for each main supported font group/language family, including English, French, German, Spanish, Portuguese, Italian, Dutch, Polish, Czech, Turkish, Greek, Russian, Ukrainian, Japanese, Chinese, and Korean.

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

### Credits

- [SuperFW](https://github.com/davidgfnet/superfw) by David Guillen Fandos: source of the matching flashcard font packs used for Latin Extended, Greek/Cyrillic, Japanese kana, CJK ideographs, and Korean Hangul coverage.
- Butano common sprite fonts: used for the small UI text.
- [dict.cc](https://www.dict.cc/): target vocabulary-export format and language-data workflow this trainer is designed around.
