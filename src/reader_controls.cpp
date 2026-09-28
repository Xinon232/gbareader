#include "reader_controls.h"
extern "C" {
#include "font_render.h"
}
namespace reader {
const char* const controls_titles[CONTROLS_PAGE_COUNT] = {
    "Controls 1/11: About", "Controls 2/11: Home", "Controls 3/11: Reading",
    "Controls 4/11: Saving", "Controls 5/11: Settings", "Controls 6/11: Arabic", "Controls 7/11: Arabic mode",
    "Controls 8/11: SAV files", "Controls 9/11: Hold controls", "Controls 10/11: Global settings",
    "Controls 11/11: Last book"
};
const char* const controls_lines[CONTROLS_PAGE_COUNT][CONTROLS_LINES] = {
    {"Read TXT and EPUB books.", "Save and resume your place.",
     "Put UTF-8 TXT/EPUB files", "in /gbareader on SD root.",
     "Text-only EPUB; no DRM.", "No text editing or export."},
    {"Up/Down: select a book.", "A: open selected book.",
     "Select: Controls.", "Start: Credits.",
     "Credits: Left/Right pages.", "Any number of files; no folders."},
    {"Right or A: next page.", "Left or B: previous page.",
     "Hold Down: settings.", "Hold Up: shoulder turns.",
     "When on: L back, R next.", "Startup default: in Settings."},
    {"Start: save place/Arabic", "and up to 64 Back pages.",
     "Failed? Retry Reader Start.", "Select: Home WITHOUT save.",
     "State/cache: book.ext.sav", "Copy both book and SAV."},
    {"Up/Down: choose setting.", "Left/Right: change value.",
     "Spacing 0-4, gap None-Full.", "Go to: A jumps, L/R 10%.",
     "B/Start: apply and return.", "Apply saves global settings."},
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
 "Release before trying again.", "L+R: On/Off flashes briefly."},
 {"Settings apply: auto-save.", "Spacing/gap: all books.",
 "L/R on startup: next run.", "Hold Up: this run only.",
 "Failed settings stay in RAM.", "Retry close or Reader Start."},
 {"Home selects last opened book.", "Selection only; A opens.",
 "Missing book? Top of list.", "Open saves its full filename.",
 "Global files: SETTINGS0.DAT", "and SETTINGS1.DAT. Back up."}
};
void draw_controls(uint8_t* pixels, int page) {
    if(page < 0 || page >= CONTROLS_PAGE_COUNT) return;
    for(int i = 0; i < CONTROLS_LINES; ++i)
        draw_text_idx8_bus16_range(controls_lines[page][i],
                                  pixels + (30 + i * 16) * 240 + 8, 0, 224, 240, 1);
}
}
