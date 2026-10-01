#include "reader_menu.h"

namespace reader {

void list_show(ListNav& nav, int count, int rows)
{
    if(count <= 0) { nav.selected = nav.top = 0; return; }
    if(nav.selected < 0) nav.selected = 0;
    if(nav.selected >= count) nav.selected = count - 1;
    if(nav.selected < nav.top) nav.top = nav.selected;
    if(nav.selected >= nav.top + rows) nav.top = nav.selected - rows + 1;
    const int last_top = count > rows ? count - rows : 0;
    if(nav.top > last_top) nav.top = last_top;
    if(nav.top < 0) nav.top = 0;
}

bool list_step(ListNav& nav, int count, int rows, int delta, bool wrap)
{
    if(count <= 0 || !delta) return false;
    int next = nav.selected + delta;
    if(next < 0) next = wrap ? count - 1 : 0;
    if(next >= count) next = wrap ? 0 : count - 1;
    if(next == nav.selected) return false;
    nav.selected = next;
    list_show(nav, count, rows);
    return true;
}

bool list_page(ListNav& nav, int count, int rows, int delta)
{
    if(count <= rows || !delta) return false;
    const int last_top = count - rows;
    int top = nav.top + (delta > 0 ? rows : -rows);
    if(top < 0) top = 0;
    if(top > last_top) top = last_top;
    if(top == nav.top) return false;
    const int slot = nav.selected - nav.top;
    nav.top = top;
    nav.selected = top + slot;
    list_show(nav, count, rows);
    return true;
}

bool Marquee::step(int text_width, int available, unsigned frame)
{
    if(text_width <= available) return false;
    if(wait) { --wait; return false; }
    if(offset >= text_width - available) {
        // Hold at the end, then back to the start.
        offset = 0;
        wait = 120;
        return true;
    }
    if(frame & 1) return false;
    ++offset;
    if(offset >= text_width - available) wait = 60;
    return true;
}

int settings_items(bool book_open, SettingsItem (&items)[SETTINGS_MAX_ROWS])
{
    int n = 0;
    if(book_open) items[n++] = SettingsItem::BACK_TO_FILES;
    if(book_open) items[n++] = SettingsItem::GOTO;
    items[n++] = SettingsItem::LINE_SPACING;
    items[n++] = SettingsItem::PARAGRAPH_GAP;
    items[n++] = SettingsItem::PAGE_TURN_KEYS;
    items[n++] = SettingsItem::ABOUT;
    return n;
}

const char* const about_titles[ABOUT_PAGE_COUNT] = {
    "About", "Controls", "License", "Text and fonts", "Arabic font", "SD card and files",
    "EPUB and engine"
};

const char* const about_lines[ABOUT_PAGE_COUNT][ABOUT_LINES] = {
    {"gbareader V3.1", "", "Made by Halim Jarrar", "(C) 2026", "", "halimj.itch.io",
     "gba@halim-jarrar.de", ""},
    {"#Books", "Put books in /gbareader", "#File list", "Start: Import books", "#Reading",
     "Start: Save position", "Hold Up: L/R page turns on/off", ""},
    {"gbareader is free software under", "the GNU GPL 3.0 or later.", "", "#Source code",
     "github.com/Xinon232/gbareader", "", "Parts made by others and their",
     "licenses are on the next pages."},
    {"#Text renderer", "SuperFW by David Guillen Fandos", "License: GPL 3.0 or later", "#Fonts",
     "UNSCII by Viznut: GPL", "GNU Unifont: GPL 2.0 or later", "with the font exception",
     "Fixedsys Excelsior: public domain"},
    {"Ghoulam Regular", "(C) 2025 Imad AlFil (mloukhiyye)", "License: CC BY 4.0", "",
     "Changed: converted to an 11 px", "bitmap font for gbareader.", "#Source",
     "mloukhiyye.itch.io"},
    {"#SD card driver", "From SuperFW", "by David Guillen Fandos", "License: GPL 3.0 or later",
     "#File system", "FatFs by ChaN", "License: FatFs (BSD style)", ""},
    {"#EPUB unpacking", "miniz (C) Rich Geldreich, RAD", "Game Tools, Valve. License: MIT",
     "#Engine", "Butano by Gustavo Valiente", "License: zlib", "#Built with",
     "devkitPro / devkitARM"}
};

}
