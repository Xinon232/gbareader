# GBA Reader v0.4.3 — Save feedback

- Pressing `Start` now displays a `save...` overlay in the existing UI font before the TXT footer is written to SD.
- The overlay is foreground UI: it appears above the reading-page bitmap and any other visible sprites.
- One frame is committed before the synchronous SD write begins, so the indicator remains visible while saving.
- The overlay is removed as soon as the existing save call returns.
- The TXT footer format and all existing save mechanics are unchanged.
