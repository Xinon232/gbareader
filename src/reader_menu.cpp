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

int settings_items(bool book_open, SettingsItem (&items)[SETTINGS_MAX_ROWS], bool comic)
{
    int n = 0;
    comic = comic && book_open;
    if(book_open) items[n++] = SettingsItem::BACK_TO_FILES;
    if(book_open) items[n++] = SettingsItem::GOTO;
    if(!comic) {
        items[n++] = SettingsItem::LINE_SPACING;
        items[n++] = SettingsItem::PARAGRAPH_GAP;
    }
    items[n++] = SettingsItem::PAGE_TURN_KEYS;
    items[n++] = SettingsItem::FILE_EXTENSIONS;
    // All comics; shown only while one is open.
    if(comic) items[n++] = SettingsItem::PAGE_MAP;
    if(comic) items[n++] = SettingsItem::ANIMATIONS;
    items[n++] = SettingsItem::ABOUT;
    // Settings of this book only, below the general ones.
    if(book_open) {
        items[n++] = SettingsItem::BOOK_LABEL;
        if(comic) {
            items[n++] = SettingsItem::VIEW;
        }
        items[n++] = SettingsItem::ANGLE;
    }
    return n;
}

int settings_skip_divider(const SettingsItem* items, int count, int selected, int delta)
{
    if(count <= 0 || selectable(items[selected])) return selected;
    int next = selected + (delta < 0 ? -1 : 1);
    if(next < 0) next = count - 1;
    if(next >= count) next = 0;
    return next;
}

void library_display_name(const char* name, bool show_extension, char* out, int size)
{
    if(size <= 0) return;
    int length = 0, dot = -1;
    for(; name[length]; ++length) if(name[length] == '.' && length > 0) dot = length;
    if(!show_extension && dot > 0) length = dot;
    if(length > size - 1) length = size - 1;
    for(int i = 0; i < length; ++i) out[i] = name[i];
    out[length] = 0;
}

const char* const about_titles[ABOUT_PAGE_COUNT] = {
    "About", "Controls", "License", "Text and fonts", "Arabic font", "SD card and files",
    "EPUB and engine"
};

const char* const about_lines[ABOUT_PAGE_COUNT][ABOUT_LINES] = {
    {"gbareader 10.0", "(c) 2026 Halim Jarrar", "GPL-3.0-or-later", "No warranty.",
     "Source:", "github.com/Xinon232/gbareader", "halimj.itch.io", "gba@halim-jarrar.de"},
    {"#File list", "Put books in /gbareader", "Start: Import books", "L+R: Sleep, hold Start 1 s: Wake", "#Reading",
     "Start: Save position", "Hold Up: L/R page turns on/off", "Comics: D-pad moves the picture"},
    {"gbareader is free software under", "the GNU GPL 3.0 or later", "(GPL-3.0-or-later).",
     "It comes with no warranty.", "#Source code", "github.com/Xinon232/gbareader",
     "Parts made by others and their", "licenses are on the next pages."},
    {"#Text renderer", "SuperFW by David Guillen Fandos", "License: GPL 3.0 or later", "#Fonts",
     "UNSCII by Viznut: GPL", "GNU Unifont: GPL 2.0 or later", "with the font exception",
     "Fixedsys Excelsior: public domain"},
    {"Ghoulam Regular", "(C) 2025 Imad AlFil (mloukhiyye)", "License: CC BY 4.0", "",
     "Changed: converted to an 11 px", "bitmap font for gbareader.", "#Source",
     "mloukhiyye.itch.io"},
    {"#SD card driver: SuperCard", "SuperFW by David Guillen Fandos", "License: GPL 3.0 or later",
     "#EverDrive and Omega drivers", "EverDrive: edio-gba by krikzz, MIT",
     "Omega: Halim Jarrar, GPL, after", "the Omega kernel (Apache 2.0)",
     "Files: FatFs by ChaN (BSD style)"},
    {"#EPUB unpacking", "miniz (C) Rich Geldreich, RAD", "Game Tools, Valve. License: MIT",
     "#Engine", "Butano by Gustavo Valiente", "License: zlib", "#Built with",
     "devkitPro / devkitARM"}
};

}
