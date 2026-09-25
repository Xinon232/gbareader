#include "reader_controls.h"
extern "C" {
#include "font_render.h"
}
namespace reader {
const char* const controls_titles[CONTROLS_PAGE_COUNT] = {
    "Controls 1/9: About", "Controls 2/9: Home", "Controls 3/9: Reading",
    "Controls 4/9: Saving", "Controls 5/9: Settings", "Controls 6/9: Arabic", "Controls 7/9: Arabic mode",
    "Controls 8/9: SAV files", "Controls 9/9: Hold controls"
};
const char* const controls_lines[CONTROLS_PAGE_COUNT][CONTROLS_LINES] = {
    {"Read TXT and EPUB books.", "Save and resume your place.",
     "Put UTF-8 TXT/EPUB files", "in /gbareader on SD root.",
     "Text-only EPUB; no DRM.", "No text editing or export."},
    {"Up/Down: select a book.", "A: open selected book.",
     "Select: Controls.", "Start: Credits.",
     "Credits: Left/Right pages.", "Up to 64 files; no folders."},
    {"Right or A: next page.", "Left or B: previous page.",
     "Hold Down: settings.", "Hold Up: shoulder turns.",
     "When on: L back, R next.", "Shoulders start off each run."},
    {"Start: save place/settings", "and up to 64 Back pages.",
     "Wait for Saved/Save failed.", "Select: Home WITHOUT save.",
     "State/cache: book.ext.sav", "Copy both book and SAV."},
    {"Up/Down: choose setting.", "Left/Right: change value.",
     "Line spacing 0 to 4, gap", "None/Small/Half/Full.",
     "B/Start: apply and return.", "Reader Start: save changes."},
    {"Arabic: joined, right to left.", "Harakat hidden, not deleted.",
     "Latin and digits stay LTR.", "Ghoulam native 11px font.",
     "Limited mixed-direction text.", "No full Persian/Urdu support."},
    {"Arabic starts OFF per book.", "Opening-page Arabic: ON.",
     "ON auto-saves in its SAV.", "Saved ON stays ON on reopen.",
     "Later Arabic stays unshaped.", "Save there; reopen to enable."},
 {"Book bytes stay unchanged.", "EPUB cache is in its SAV.",
 "First cache saves itself.", "Keep both files on rename.",
 "Save failed? Keep open.", "Retry Start; keep backups."},
 {"Hold Up/Down alone: 48 frames.", "About 0.8s, same as Caps.",
 "Short press: no action.", "Other keys cancel the hold.",
 "Release before trying again.", "L+R: On/Off flashes briefly."}
};
void draw_controls(uint8_t* pixels, int page) {
    if(page < 0 || page >= CONTROLS_PAGE_COUNT) return;
    for(int i = 0; i < CONTROLS_LINES; ++i)
        draw_text_idx8_bus16_range(controls_lines[page][i],
                                  pixels + (30 + i * 16) * 240 + 8, 0, 224, 240, 1);
}
}
