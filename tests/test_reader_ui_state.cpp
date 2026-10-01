#include "reader_ui_state.h"
#include "reader_menu.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace reader;

static void test_list_nav()
{
    constexpr int ROWS = 8;
    ListNav nav{};
    list_show(nav, 0, ROWS);
    assert(nav.selected == 0 && nav.top == 0); // Empty or unavailable storage.
    assert(!list_step(nav, 0, ROWS, 1, true) && !list_page(nav, 0, ROWS, 1));
    for(int count = 1; count <= 70; ++count) {
        for(int selected = 0; selected < count; ++selected) {
            ListNav n{selected, 0};
            list_show(n, count, ROWS);
            assert(n.top >= 0 && n.top <= selected && selected < n.top + ROWS);
            assert(n.top + ROWS <= count || n.top == 0);
        }
        // A fresh press wraps, a held repeat stops at the ends.
        ListNav n{};
        assert(list_step(n, count, ROWS, -1, true) == (count > 1));
        assert(n.selected == count - 1 && n.selected < n.top + ROWS);
        assert(list_step(n, count, ROWS, 1, true) == (count > 1) && n.selected == 0 && n.top == 0);
        assert(!list_step(n, count, ROWS, -1, false) && n.selected == 0);
        n.selected = count - 1; list_show(n, count, ROWS);
        assert(!list_step(n, count, ROWS, 1, false) && n.selected == count - 1);
    }
    // Moving down scrolls one row at a time once the cursor reaches the bottom.
    nav = {};
    for(int i = 1; i < 8; ++i) { assert(list_step(nav, 20, ROWS, 1, false)); assert(nav.top == 0); }
    assert(list_step(nav, 20, ROWS, 1, false) && nav.selected == 8 && nav.top == 1);
    // Left / Right: one page, the cursor keeps its screen row; nothing at the ends.
    nav = {2, 0};
    assert(!list_page(nav, 20, ROWS, -1));
    assert(list_page(nav, 20, ROWS, 1) && nav.top == 8 && nav.selected == 10);
    assert(list_page(nav, 20, ROWS, 1) && nav.top == 12 && nav.selected == 14);
    assert(!list_page(nav, 20, ROWS, 1));
    assert(list_page(nav, 20, ROWS, -1) && nav.top == 4 && nav.selected == 6);
    assert(!list_page(nav, ROWS, ROWS, 1)); // Short lists have one page.
}

static void test_key_repeat()
{
    KeyRepeat k;
    assert(k.update(true, true) == 1);
    int repeats = 0, first = -1;
    for(int frame = 1; frame <= LIST_REPEAT_DELAY + 4 * LIST_REPEAT_PERIOD; ++frame)
        if(k.update(true, false) == 2) { if(first < 0) first = frame; ++repeats; }
    assert(first == LIST_REPEAT_DELAY && repeats == 5); // 0.4 s, then 7.5 per second.
    assert(k.update(false, false) == 0 && k.frames == 0);
    assert(k.update(true, false) == 0); // Holding after a release needs a new press.
}

static void test_marquee()
{
    Marquee m;
    for(unsigned f = 0; f < 200; ++f) assert(!m.step(100, 100, f)); // Fits: never moves.
    m.reset();
    unsigned f = 0;
    for(; f < 60; ++f) assert(!m.step(120, 100, f)); // One second still.
    int moves = 0;
    while(m.offset < 20) { if(m.step(120, 100, f++)) ++moves; assert(f < 400); }
    assert(moves == 20);
    int still = 0;
    while(!m.step(120, 100, f++)) ++still;
    assert(still >= 60 && m.offset == 0); // Hold at the end, then back to the start.
}

static void test_settings_items()
{
    SettingsItem items[SETTINGS_MAX_ROWS];
    assert(settings_items(false, items) == 5);
    assert(items[0] == SettingsItem::LINE_SPACING && items[1] == SettingsItem::PARAGRAPH_GAP &&
           items[2] == SettingsItem::PAGE_TURN_KEYS && items[3] == SettingsItem::FILE_EXTENSIONS &&
           items[4] == SettingsItem::ABOUT);
    assert(settings_items(true, items) == 7);
    assert(items[0] == SettingsItem::BACK_TO_FILES && items[1] == SettingsItem::GOTO &&
           items[2] == SettingsItem::LINE_SPACING && items[3] == SettingsItem::PARAGRAPH_GAP &&
           items[4] == SettingsItem::PAGE_TURN_KEYS && items[5] == SettingsItem::FILE_EXTENSIONS &&
           items[6] == SettingsItem::ABOUT);
}

static void test_library_display_name()
{
    char out[16];
    library_display_name("Book.txt", true, out, sizeof(out)); assert(!strcmp(out, "Book.txt"));
    library_display_name("Book.txt", false, out, sizeof(out)); assert(!strcmp(out, "Book"));
    library_display_name("A.b.c.epub", false, out, sizeof(out)); assert(!strcmp(out, "A.b.c"));
    library_display_name(".hidden", false, out, sizeof(out)); assert(!strcmp(out, ".hidden"));
    library_display_name("noext", false, out, sizeof(out)); assert(!strcmp(out, "noext"));
    library_display_name("A very long name.txt", true, out, sizeof(out)); assert(!strcmp(out, "A very long nam"));
}

int main()
{
    test_list_nav();
    test_key_repeat();
    test_marquee();
    test_settings_items();
    test_library_display_name();

    SaveMessageTimer timer{};
    assert(! save_message_visible(timer));

    start_save_message(timer);
    assert(save_message_visible(timer));
    for(int frame = 1; frame < SAVE_MESSAGE_FRAMES; ++frame) {
        assert(! tick_save_message(timer));
        assert(save_message_visible(timer));
    }
    assert(tick_save_message(timer));
    assert(! save_message_visible(timer));
    assert(! tick_save_message(timer));

    start_save_message(timer);
    cancel_save_message(timer);
    assert(! save_message_visible(timer));
    std::puts("PASS: list wrap/scroll/paging, hold repeat, marquee, Settings rows, transient save message timer");
}
