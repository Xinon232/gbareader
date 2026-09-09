#include "reader_credits.h"
extern "C" {
#include "font_render.h"
}
namespace reader {
const char* const credits_titles[CREDITS_PAGE_COUNT] = {
    "Credits 1/7: Author", "Credits 2/7: Ghoulam", "Credits 3/7: SuperFW",
    "Credits 4/7: UNSCII", "Credits 5/7: Unifont", "Credits 6/7: Framework",
    "Credits 7/7: Sources"
};
const char* const credits_lines[CREDITS_PAGE_COUNT][CREDITS_LINES] = {
    {"Made by Halim Jarrar", "(C) 2026", "halim-jarrar.de", "monday@halim-jarrar.de", "", ""},
    {"Ghoulam Regular (C) 2025", "Imad AlFil / mloukhiyye", "CC BY 4.0", "mloukhiyye.itch.io",
     "GSUB to native 11px bitmap.", "Source/license links: manual."},
    {"SuperFW fonts and renderer", "David Guillen Fandos", "(C) 2024-2025", "GPL v3 or later",
     "superfw.davidgf.net", "Font pack and SD foundation."},
    {"UNSCII by Viznut", "unscii-16-full: GPL", "Includes GNU Unifont and", "Fixedsys Excelsior glyphs.",
     "Fixedsys: public domain.", "viznut.fi/unscii"},
    {"GNU Unifont / Hangul", "Roman Czyborra, Paul Hardy", "and Unifont contributors", "GPL v2+ with font exception",
     "unifoundry.com/unifont", "Full notices in source/manual."},
    {"Butano engine and UI font", "Gustavo Valiente - zlib", "devkitARM / devkitPro", "FatFs (C) 2022 ChaN",
     "Permissive FatFs license.", "See source license notices."},
    {"miniz - MIT license", "Rich Geldreich / Tenacious", "RAD Game Tools / Valve", "Based on gba-vocab-trainer-CC",
     "v0.2.5; project GPL v3+", "github.com/Xinon232/gbareader"}
};
void draw_credits(uint8_t* pixels, int page)
{
    if(page < 0 || page >= CREDITS_PAGE_COUNT) return;
    for(int i = 0; i < CREDITS_LINES; ++i) {
        const char* text = credits_lines[page][i];
        if(!text[0]) continue;
        const unsigned width = font_width(text);
        const int y = page == 0 ? 40 + i * 22 : 30 + i * 16;
        draw_text_idx8_bus16_range(text, pixels + y * 240 + (240 - width) / 2,
                                  0, width, 240, 1);
    }
}
}
