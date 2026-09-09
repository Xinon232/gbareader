#include "reader_controls.h"
extern "C" {
#include "font_render.h"
}
namespace reader {
const char* const controls_titles[CONTROLS_PAGE_COUNT] = {
    "Controls 1/6: About", "Controls 2/6: Home", "Controls 3/6: Reading",
    "Controls 4/6: Saving", "Controls 5/6: Settings", "Controls 6/6: Arabic"
};
const char* const controls_lines[CONTROLS_PAGE_COUNT][CONTROLS_LINES] = {
    {"Read TXT and EPUB books.", "Save and resume your place.",
     "Put UTF-8 TXT/EPUB files", "in /gbareader on SD root.",
     "Text-only EPUB; no DRM.", "No text editing or export."},
    {"Up/Down: select a book.", "A: open selected book.",
     "Select: Controls.", "Start: Credits.",
     "Credits: Left/Right pages.", "Up to 64 files; no folders."},
    {"Right or A: next page.", "Left or B: previous page.",
     "Down: reader settings.", "Up: toggle shoulder turns.",
     "When on: L back, R next.", "Shoulders start off each run."},
    {"Start: save place/settings", "and up to 64 Back pages.",
     "Wait for Saved/Save failed.", "Select: Home WITHOUT save.",
     "State stays in TXT/EPUB.", "No .sav or settings file."},
    {"Up/Down: choose setting.", "Left/Right: change 1 to 4.",
     "Line spacing, top margin,", "bottom margin. Size fixed.",
     "B/Start: apply and return.", "Reader Start: save changes."},
    {"Arabic: joined, right to left.", "Harakat hidden, not deleted.",
     "Latin and digits stay LTR.", "Ghoulam native 11px font.",
     "Limited mixed-direction text.", "No full Persian/Urdu support."}
};
void draw_controls(uint8_t* pixels, int page) {
    if(page < 0 || page >= CONTROLS_PAGE_COUNT) return;
    for(int i = 0; i < CONTROLS_LINES; ++i)
        draw_text_idx8_bus16_range(controls_lines[page][i],
                                  pixels + (30 + i * 16) * 240 + 8, 0, 224, 240, 1);
}
}
