gbcomic converter for gbareader (Linux, x86-64)
===============================================

Turns comics into .gbcomic files for gbareader on the Game Boy Advance.
Reads PDF, EPUB (comic EPUBs: their pictures), CBZ (or ZIP of pictures),
a folder of pictures, or one picture. One program, nothing to install
(glibc 2.28 or newer: most Linux systems from 2019 on).

    chmod +x gbcomic      (once, or tick "Allow executing" in its properties)
    ./gbcomic             (or double-click it; comics can be given too)

In the window:
  1. Add comics (or a folder of pictures).
  2. Choose the angles: both, upright only (Normal, Upside down) or
     sideways only (90°).
  3. Tick the zooms you want (as many as you like) for each angle:
       upright   1.1 to 3.0 (default 1.6, 1.8, 2.0)
       sideways  1.1 to 3.0 (default 2.2, 2.4, 2.6)
     Fit page and Fit width are always included. Preview... shows a page
     at a zoom on the GBA screen (240 x 160) before you convert: pages,
     A / B, D-pad and mouse to move around, page map.
  4. Press Convert. The .gbcomic files appear next to the comics (or in
     the folder you chose). Copy them to /gbareader on the SD card.

Source: gbcomic_gui.py (the window) and gbcomic.py (the converter; with
Python 3.8+ and  pip install pillow pypdfium2  it also runs on the command
line: python3 gbcomic.py --help).
Built with PyInstaller from Python 3.12 (PSF license), Tcl/Tk (BSD),
Pillow (MIT-CMU) and pypdfium2 / PDFium (Apache 2.0 / BSD). gbcomic is
part of gbareader (GPL 3.0 or later): https://github.com/Xinon232/gbareader
