# Emulator UI screenshots (optional)

`screens.py` runs the exact ROM on mGBA with a register-level Supercard SD
model (`scsd_model.h`, copied from gbamp3 with two gbareader adjustments:
the write-access mode bit and the CMD3 card state) and scripted buttons, then
writes contact sheets of every screen, including a real import onto the card
image.

    sudo apt-get install libmgba-dev mtools dosfstools && pip install pillow
    python3 tests/emu/screens.py gbareader.gba /tmp/gbareader-screens

This is a UI check on a modeled card, not physical Supercard validation. It is
not part of CI (the devkitARM image has no mGBA).
