#pragma once
// Menu and list behaviour shared by Home, Import and Settings (gbamp3 v1.8 rules).
#include <cstdint>

namespace reader {

// A list of count entries showing rows of them from top.
struct ListNav {
    int selected = 0;
    int top = 0;
};
// Keeps selected inside the list and visible.
void list_show(ListNav& nav, int count, int rows);
// Up / Down: one entry. A fresh press wraps around the ends, a held repeat
// stops there. Returns true when the selection moved.
bool list_step(ListNav& nav, int count, int rows, int delta, bool wrap);
// Left / Right: one page; the cursor keeps its row on the screen. Nothing on
// the first / last page.
bool list_page(ListNav& nav, int count, int rows, int delta);

// gbamp3 hold-to-repeat: a press fires at once, holding fires again after
// 0.4 s and then 7.5 times per second.
constexpr int LIST_REPEAT_DELAY = 24;
constexpr int LIST_REPEAT_PERIOD = 8;
struct KeyRepeat {
    int frames = 0;
    // Returns 1 for a press, 2 for a repeat, 0 otherwise.
    int update(bool held, bool pressed)
    {
        if(pressed) { frames = 0; return 1; }
        if(!held) { frames = 0; return 0; }
        ++frames;
        return frames >= LIST_REPEAT_DELAY && (frames - LIST_REPEAT_DELAY) % LIST_REPEAT_PERIOD == 0 ? 2 : 0;
    }
};

// Selected-row marquee (gbamp3): wait 1 s, scroll 1 px every second frame
// to the end, hold 1 s, start again.
struct Marquee {
    int offset = 0;
    int wait = 60;
    void reset() { offset = 0; wait = 60; }
    // Returns true when the offset changed.
    bool step(int text_width, int available, unsigned frame);
};

// Settings rows. Back to Files (first) and Go to only exist while a book is open.
enum class SettingsItem : uint8_t {
    GOTO, LINE_SPACING, PARAGRAPH_GAP, PAGE_TURN_KEYS, FILE_EXTENSIONS, BACK_TO_FILES, ABOUT
};
constexpr int SETTINGS_MAX_ROWS = 7;
int settings_items(bool book_open, SettingsItem (&items)[SETTINGS_MAX_ROWS]);

// Home list name: without show_extension, the part from the last '.' is cut
// (a leading '.' stays). Writes at most size bytes, terminator included.
void library_display_name(const char* name, bool show_extension, char* out, int size);

// About: one topic per page.
constexpr int ABOUT_PAGE_COUNT = 7;
constexpr int ABOUT_LINES = 8;
extern const char* const about_titles[ABOUT_PAGE_COUNT];
// A line starting with '#' is a small grey label (the '#' is not shown).
extern const char* const about_lines[ABOUT_PAGE_COUNT][ABOUT_LINES];

}
