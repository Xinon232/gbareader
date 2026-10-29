# gbcomic converter

Offered at [halimj.itch.io](https://halimj.itch.io/). Part of gbareader (GPL 3.0 or later, no warranty); its source (`tools/gbcomic.py`, `tools/gbcomic_gui.py`, `tools/gbcomic_preview.py`, `tools/converter/`) is published with gbareader's at [github.com/Xinon232/gbareader](https://github.com/Xinon232/gbareader). Rebuild the PDFs with `python3 docs/build_guide_pdf.py` and `python3 docs/build_controls_pdf.py` (ReportLab and pypdf).

## Converting comics

`tools/gbcomic.py` turns comics into `.gbcomic` files. It reads PDF, EPUB (comic EPUBs: their pictures, in reading order), CBZ (or a ZIP of pictures), a folder of pictures or a single picture.

Ready-made programs with a window, nothing to install (built by `tools/converter/build.sh`):

- **Windows** (`gbcomic-windows-x64.zip`): start `gbcomic.exe`, or drop comics on it.
- **Linux** (`gbcomic-linux-x86_64.zip`, glibc 2.28 or newer): start `gbcomic`.

In the window: add comics (or a folder of pictures), choose the angles (both, upright only, sideways only), tick any zooms for each angle, 1.1 to 3.0 (the window shows the size per page; `Preview…` shows a page at a zoom on the GBA screen first), choose where to save (next to each comic, or a folder) and press Convert. The picture options (sharpening, margin crop, spreads, dithering, lettering, PDF resolution) are there too. `tools/gbcomic_gui.py` is the window; the command line below does the same.

`Preview…` (in the window) shows one page of the comic chosen at one zoom, made by the converter's own code and cut into screens as the reader does: 240 × 160 pixels (160 × 240 sideways), shown 1× to 4× larger (2× to start). `< Page` / `Page >` (or type a page), `A` / `B` (or click the right / left half), the D-pad buttons, dragging or the mouse wheel move around it; reading order, Skip empty and the page map as on the GBA. Keys: arrows, X / Enter (A), Z / Backspace (B), Page Up / Down. Nothing is written until Convert.

From source it needs Python 3.8 or newer, Pillow and, for PDF, pypdfium2 (`pip install pillow pypdfium2`; `pdftoppm` from poppler-utils works too); the window also needs Tkinter:

```sh
python3 tools/gbcomic.py --ask "My Comic.pdf" Other.epub   # asks for angles and zooms
python3 tools/gbcomic.py "My Comic.cbz"                   # defaults: writes My Comic.gbcomic next to it
python3 tools/gbcomic.py book.pdf -o Book.gbcomic --angles sideways --sideways 2.3,2.5
```

Copy the `.gbcomic` into `/gbareader` (or import it with `Start`). Defaults: Fit page, Fit width, 1.6x, 1.8x and 2x for the upright angles, Fit page, Fit width, 2.2x, 2.4x and 2.6x for the sideways ones, light sharpening, light margin crop: about 1.2 MB per page for a colour comic (one kind of angle only: about 0.6 MB). Options:

- `--normal 1.6,1.8,2.0`: upright zooms, any of 1.1 to 3.0 in steps of 0.1.
- `--sideways 2.2,2.4,2.6`: sideways (90°) zooms, any of 1.1 to 3.0 in steps of 0.1. Fit page and Fit width (1.0) are always there. Each zoom adds about its size squared (2x is four times Fit width).
- `--angles both|upright|sideways`: screens for Normal and Upside down (upright), 90° right and left (sideways), or both.
- `--ask`: ask for the angles and zooms first (in a terminal); several comics take the same answers.
- `--dither`: smoother colour gradients (a little larger; can look grainy on line art).
- `--sharpen off|light|strong`, `--boost-lettering` (keeps small lettering dark).
- `--margins off|light|strong`: crop blank paper (light), also page numbers and scan edges (strong).
- `--spreads auto|always|never`: wide pages as two halves at Fit page.
- `--dpi 200`: resolution for PDF pages; `--verify`: check every packed piece.

