// gbareader 10.0 -- streaming Supercard SD TXT/EPUB reader.

#include "bn_bg_palette_item.h"
#include "bn_bg_palette_ptr.h"
#include "bn_core.h"
#include "bn_core_lock.h"
#include "bn_keypad.h"
#include "bn_memory.h"
#include "bn_optional.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_items_ui_small_font_box.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_affine_mat_ptr.h"
#include "bn_sprite_double_size_mode.h"
#include "bn_sprite_ptr.h"
#include "bn_string.h"
#include "bn_timer.h"
#include "bn_timers.h"
#include "bn_vector.h"

#include "ui_small_font.h"
extern "C" {
#include "font_render.h"
}
#include "reader_core.h"
#include "reader_body.h"
#include "reader_open.h"
#include "reader_ui_state.h"
#include "reader_hold.h"
#include "reader_menu.h"
#include "reader_screen.h"
#include "reader_browse.h"
#include "epub_document.h"
#include "reader_file.h"
#include "reader_global_settings.h"
#include "reader_turn.h"
#include "reader_keys.h"
#include "gbcomic.h"
#include "comic_reader.h"

#include <cstring>
#include <new>

namespace {

using reader::Scene;
namespace screen = reader::screen;

// 0-3: book page (white, black, Arabic greys); 4-5: gbamp3 light blue and grey.
// 8-255: the colours of the comic picture on screen.
struct ScreenColors { bn::color colors[256]; };
constexpr ScreenColors make_screen_colors()
{
    ScreenColors result{};
    const bn::color ui[6] = {
        bn::color(31, 31, 31), bn::color(0, 0, 0), bn::color(12, 12, 12), bn::color(20, 20, 20),
        bn::color(21, 26, 31), bn::color(16, 16, 16)
    };
    for(int i = 0; i < 6; ++i) result.colors[i] = ui[i];
    return result;
}
constexpr ScreenColors initial_colors = make_screen_colors();
constexpr bn::bg_palette_item palette_item(bn::span<const bn::color>(initial_colors.colors), bn::bpp_mode::BPP_8);
BN_DATA_EWRAM_BSS ScreenColors comic_colors;

BN_DATA_EWRAM_BSS reader::ReaderFile file;
BN_DATA_EWRAM_BSS reader::GlobalSettingsStore global_settings;
static_assert(reader::GLOBAL_BOOK_NAME_MAX == reader::LIBRARY_NAME_MAX);
// One block of memory for the open book: the EPUB reader and the turned-page
// canvas. A comic reads and unpacks at its start; the rest keeps the decoded
// part of the picture on screen.
constexpr size_t EPUB_BYTES = (sizeof(reader::EpubDocument) + 7) & ~size_t(7);
// An open book's memory: an EPUB and the turned-page canvas, or a comic's
// pictures (all of it). 16 KiB more than they need: what Import copied
// through before (it now borrows this; no book is open then), so a comic
// keeps more of its picture around the screen.
constexpr size_t BOOK_MEMORY_BYTES = EPUB_BYTES + reader::TURN_CANVAS_BYTES + 16 * 1024;
static_assert(EPUB_BYTES >= reader::gbcomic::Book::ARENA_BYTES, "comic working memory");
alignas(8) BN_DATA_EWRAM_BSS uint8_t book_memory[BOOK_MEMORY_BYTES];
reader::EpubDocument& epub = *::new(static_cast<void*>(book_memory)) reader::EpubDocument();
// After a comic used the memory: a fresh EPUB reader.
void reset_epub() { ::new(static_cast<void*>(book_memory)) reader::EpubDocument(); }
BN_DATA_EWRAM_BSS reader::gbcomic::Book comic;
// The comic page on screen.
BN_DATA_EWRAM_BSS reader::ComicReader comic_reader;
bool comic_open = false;
// This book's own settings ("This book" in Settings).
reader::BookPrefs book_prefs{};
BN_DATA_EWRAM_BSS reader::Page page;
BN_DATA_EWRAM_BSS reader::PageHistory history;
BN_DATA_EWRAM_BSS reader::PageHistoryRebuild history_rebuild;
BN_DATA_EWRAM_BSS reader::PageCount page_count;
// Page and states too large for the IWRAM stack.
BN_DATA_EWRAM_BSS reader::Page next_page_buffer;
BN_DATA_EWRAM_BSS reader::TxtSaveFooter loaded_state;
BN_DATA_EWRAM_BSS reader::TxtSaveFooter work_state;
// The last state written to the book's .sav (or the state it opened in): per-book
// settings are saved with it, so changing them never saves a newer position.
BN_DATA_EWRAM_BSS reader::TxtSaveFooter saved_state;
// A turned book (or its Settings) is drawn here and turned onto the screen.
uint8_t* const turn_canvas = book_memory + EPUB_BYTES;
reader::Settings settings;
// Pages turned since page_count started; page number = counted pages + 1 + turns.
int page_turns = 0;

void restart_page_count()
{
    reader::begin_page_count(page.start_offset, page_count);
    page_turns = 0;
}

// Go to with unchanged settings: keep the pages already counted.
void retarget_page_count()
{
    reader::retarget_page_count(page.start_offset, page_count);
    page_turns = 0;
}

// Background page layouts (Back history, page number) may use this much of each idle frame.
constexpr int BACKGROUND_WORK_TICKS = bn::timers::ticks_per_frame() / 2;
// A comic loads ahead after drawing, until this much of the frame went by
// (and only what fits in the rest): a sliding screen keeps 60 frames a second.
constexpr int COMIC_PREFETCH_TICKS = bn::timers::ticks_per_frame() * 7 / 10;

constexpr int UI_SPRITE_CAPACITY = 24;
constexpr int SAVE_OVERLAY_SPRITE_CAPACITY = 24;
constexpr int MESSAGE_FRAMES = 120;
constexpr int IMPORT_DEPTH = 16;

int glyph_width_lookup(uint32_t cp)
{
    char text[5]{};
    if(cp < 0x80) text[0] = char(cp);
    else if(cp < 0x800) {
        text[0] = char(0xC0 | (cp >> 6)); text[1] = char(0x80 | (cp & 0x3F));
    } else if(cp < 0x10000) {
        text[0] = char(0xE0 | (cp >> 12)); text[1] = char(0x80 | ((cp >> 6) & 0x3F));
        text[2] = char(0x80 | (cp & 0x3F));
    } else {
        text[0] = char(0xF0 | (cp >> 18)); text[1] = char(0x80 | ((cp >> 12) & 0x3F));
        text[2] = char(0x80 | ((cp >> 6) & 0x3F)); text[3] = char(0x80 | (cp & 0x3F));
    }
    return int(font_width(text));
}

// Page layout asks for every character's width; ASCII answers are cached (width + 1).
int glyph_width(uint32_t cp)
{
    static uint8_t ascii_widths[128];
    if(cp >= 128) return glyph_width_lookup(cp);
    if(!ascii_widths[cp]) ascii_widths[cp] = uint8_t(glyph_width_lookup(cp) + 1);
    return ascii_widths[cp] - 1;
}

void draw_page(bn::palette_bitmap_bg_painter& painter)
{
    const bool turned = settings.angle != reader::Angle::NORMAL;
    const int pitch = reader::page_width(settings);
    uint8_t* pixels;
    if(turned) {
        pixels = turn_canvas;
        bn::memory::clear(reader::TURN_CANVAS_BYTES, *turn_canvas);
    } else {
        painter.fill(0);
        pixels = reinterpret_cast<uint8_t*>(painter.page().data());
    }
    int y = reader::page_top(settings);
    for(int line = 0; line < page.line_count; ++line) {
        if(page.lines[line].text[0])
            reader::draw_body_line(
                    page.lines[line].text,
                    pixels + y * pitch + reader::BODY_SIDE_MARGIN, settings.arabic_shaping,
                    pitch - 2 * reader::BODY_SIDE_MARGIN, pitch);
        if(line + 1 < page.line_count) {
            y += reader::FONT_HEIGHT + settings.line_spacing;
            if(page.lines[line].paragraph_break) y += reader::paragraph_gap_pixels(settings);
        }
    }
    if(turned) reader::turn_blit(turn_canvas, painter.page().data(), settings.angle);
    painter.flip_page_later();
}

bool epub_name(const char* name)
{
    int length = 0;
    while(name && name[length]) ++length;
    if(length <= 5) return false;
    const char* ext = name + length - 5;
    const char expected[] = ".epub";
    for(int i = 0; i < 5; ++i) {
        char c = ext[i];
        if(c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
        if(c != expected[i]) return false;
    }
    return true;
}

// One rotation shared by every overlay glyph of a turned book.
bn::optional<bn::sprite_affine_mat_ptr> overlay_mat;

void show_overlay(bn::sprite_text_generator& generator,
                  bn::ivector<bn::sprite_ptr>& sprites,
                  const char* text, int y = 64)
{
    sprites.clear();
    // A turned book: lay the label out as the reader sees the page, then move
    // and turn each glyph onto the screen.
    const reader::Angle angle = settings.angle;
    const int half_w = reader::turned_width(angle) / 2, half_h = reader::turned_height(angle) / 2;
    const int x = half_w - 8;
    if(angle != reader::Angle::NORMAL) y = y * (half_h - 16) / 64;
    generator.set_right_alignment();
    generator.set_bg_priority(0);
    generator.set_z_order(-32767);
    // White-box glyphs, one sprite each, join into one label over book text; the box
    // font's blank '~' keeps spaces inside the box.
    bn::string<32> boxed(text);
    for(char& c : boxed) if(c == ' ') c = '~';
    generator.set_one_sprite_per_character(true);
    generator.generate(x, y, boxed, sprites);
    generator.set_one_sprite_per_character(false);
    if(angle != reader::Angle::NORMAL) {
        if(!overlay_mat) overlay_mat = bn::sprite_affine_mat_ptr::create();
        overlay_mat->set_rotation_angle(angle == reader::Angle::RIGHT ? 90 :
                                        angle == reader::Angle::LEFT ? 270 : 180);
    }
    for(bn::sprite_ptr& sprite : sprites) {
        sprite.put_above();
        if(angle == reader::Angle::NORMAL) continue;
        const bn::fixed_point at = sprite.position();
        sprite.set_affine_mat(*overlay_mat);
        sprite.set_double_size_mode(bn::sprite_double_size_mode::ENABLED);
        if(angle == reader::Angle::RIGHT) sprite.set_position(at.y(), -at.x());
        else if(angle == reader::Angle::LEFT) sprite.set_position(-at.y(), at.x());
        else sprite.set_position(-at.x(), -at.y());
    }
    generator.set_z_order(0);
    generator.set_center_alignment();
}

void show_saving_overlay(bn::sprite_text_generator& generator,
                         bn::vector<bn::sprite_ptr, SAVE_OVERLAY_SPRITE_CAPACITY>& sprites)
{
    show_overlay(generator, sprites, "save...");
}

// Keep keypad temporaries out of the existing reader main-stack budget.
[[gnu::noinline]] unsigned sample_reader_hold(reader::ReaderHold& hold, Scene scene)
{
    const unsigned keys = unsigned(rkey::up_held()) |
            (unsigned(rkey::down_held()) << 1) |
            (unsigned(rkey::left_held()) << 2) |
            (unsigned(rkey::right_held()) << 3) |
            (unsigned(rkey::a_held()) << 4) |
            (unsigned(rkey::b_held()) << 5) |
            (unsigned(rkey::start_held()) << 6) |
            (unsigned(rkey::select_held()) << 7) |
            (unsigned(rkey::l_held()) << 8) |
            (unsigned(rkey::r_held()) << 9);
    // Comics: the D-pad moves around the picture, holding Up is no L/R switch.
    return hold.update(keys, scene == Scene::READER && !comic_open);
}
// End reader hold sampler.

// Up / Down (with hold repeat) and Left / Right paging for a list screen.
struct ListKeys {
    reader::KeyRepeat up, down, left, right;
    void reset() { up = {}; down = {}; left = {}; right = {}; }
    // Returns true when the selection moved.
    bool update(reader::ListNav& nav, int count, bool paging)
    {
        bool moved = false;
        const int u = up.update(rkey::up_held(), rkey::up_pressed());
        const int d = down.update(rkey::down_held(), rkey::down_pressed());
        if(u) moved |= reader::list_step(nav, count, screen::rows(), -1, u == 1);
        else if(d) moved |= reader::list_step(nav, count, screen::rows(), 1, d == 1);
        if(paging) {
            const int l = left.update(rkey::left_held(), rkey::left_pressed());
            const int r = right.update(rkey::right_held(), rkey::right_pressed());
            if(l) moved |= reader::list_page(nav, count, screen::rows(), -1);
            else if(r) moved |= reader::list_page(nav, count, screen::rows(), 1);
        }
        return moved;
    }
};

uint8_t* page_pixels(bn::palette_bitmap_bg_painter& painter)
{
    return reinterpret_cast<uint8_t*>(painter.page().data());
}

// Starts the next picture from the one on screen (mode 4 pages are 0xA000
// bytes apart), so a single row can be repainted.
void copy_shown_page(bn::palette_bitmap_bg_painter& painter)
{
    uint16_t* hidden = painter.page().data();
    const auto* shown = reinterpret_cast<const uint16_t*>(uintptr_t(hidden) ^ 0xA000u);
    bn::memory::copy(*shown, 240 * 160 / 2, *hidden);
}

const char* library_label(int index)
{
    const char* name = reader::library_name(index); // null only if /gbareader cannot be reread
    if(!name) return "?";
    static char shown[reader::LIBRARY_NAME_MAX];
    reader::library_display_name(name, global_settings.values.show_extensions, shown, sizeof(shown));
    return shown;
}

const char* import_label(int index)
{
    const char* name = reader::browse_name(index);
    return name ? name : "?";
}

// A comic shows the screens made for the angle chosen, upright or sideways
// (a file may have only one kind).
bool comic_angle_ok(reader::Angle angle)
{
    return reader::sideways(angle) ? comic.has_sideways() : comic.has_upright();
}
// The Angle screen: an angle a comic has no screens for is grey and skipped.
bool angle_choosable(reader::Angle angle) { return !comic_open || comic_angle_ok(angle); }
// Up / Down on the Angle screen: the next angle that can be chosen. A fresh
// press wraps around the ends, a held repeat stops there.
bool angle_step(reader::ListNav& nav, int delta, bool wrap)
{
    int next = nav.selected;
    for(int i = 1; i < reader::ANGLE_COUNT; ++i) {
        next += delta;
        if(next < 0 || next >= reader::ANGLE_COUNT) {
            if(!wrap) return false;
            next = (next + reader::ANGLE_COUNT) % reader::ANGLE_COUNT;
        }
        if(angle_choosable(reader::Angle(next))) { nav.selected = next; return true; }
    }
    return false;
}
bool comic_sideways() { return reader::sideways(settings.angle) ? comic.has_sideways() : !comic.has_upright(); }
// Each angle has its own zooms (upright and sideways screens are made apart).
uint8_t& comic_zoom() { return comic_sideways() ? book_prefs.side_zoom : book_prefs.zoom; }
bool zoom_for(int zoom, bool sideways)
{
    return zoom >= 0 && zoom < comic.zoom_count() && comic.zoom(zoom).sideways == sideways;
}
// A book seen first: the first zoom larger than Fit width (Fit page if none).
uint8_t first_zoom(bool sideways)
{
    int fit = -1;
    for(int z = 0; z < comic.zoom_count(); ++z) {
        if(comic.zoom(z).sideways != sideways) continue;
        if(comic.zoom(z).kind == reader::gbcomic::ZoomKind::WIDTH && comic.zoom(z).factor > 100) return uint8_t(z);
        if(fit < 0) fit = z;
    }
    return uint8_t(fit < 0 ? 0 : fit);
}
// Columns and rows: the book's choice, for every zoom and both angles (Auto
// shows the number each zoom needs in brackets).
uint8_t zoom_counts(int) { return book_prefs.counts; }
void choose_zoom(uint8_t zoom) { comic_zoom() = zoom; }
// The Animations screen: the D-pad, A / B along a row, A / B to the next row.
constexpr int ANIMATION_ROWS = 3;
constexpr const char* animation_names[ANIMATION_ROWS] = {"D-pad scrolling", "A/B same row", "A/B next row"};
reader::Slide& animation_slide(int row)
{
    return row == 0 ? global_settings.values.slide_pad : row == 1 ? global_settings.values.slide_ab : global_settings.values.slide_row;
}
const char* slide_name(reader::Slide slide)
{
    return slide == reader::Slide::SMOOTH ? "Smooth" : slide == reader::Slide::SLIDE ? "Slide" : "Static";
}

// View screen rows: the zooms for this angle, "Reading order", the orders,
// "Screens", screens across (Columns) and down (Rows), Skip empty.
enum class ViewRowKind : uint8_t { ZOOM, ORDER_LABEL, ORDER, SCREENS_LABEL, COLUMNS, ROWS, SKIP_EMPTY };
struct ViewRow { ViewRowKind kind; uint8_t value; };
constexpr int VIEW_ROWS_MAX = reader::gbcomic::MAX_ZOOMS + reader::gbcomic::ORDER_COUNT + 5;
int view_rows(ViewRow (&rows)[VIEW_ROWS_MAX])
{
    using namespace reader::gbcomic;
    int n = 0;
    const bool side = comic_sideways();
    for(int z = 0; z < comic.zoom_count(); ++z)
        if(comic.zoom(z).sideways == side) rows[n++] = {ViewRowKind::ZOOM, uint8_t(z)};
    rows[n++] = {ViewRowKind::ORDER_LABEL, 0};
    for(int o = 0; o < ORDER_COUNT; ++o) rows[n++] = {ViewRowKind::ORDER, uint8_t(o)};
    rows[n++] = {ViewRowKind::SCREENS_LABEL, 0};
    rows[n++] = {ViewRowKind::COLUMNS, 0};
    rows[n++] = {ViewRowKind::ROWS, 0};
    rows[n++] = {ViewRowKind::SKIP_EMPTY, 0};
    return n;
}
bool view_row_label(const ViewRow& row) { return row.kind == ViewRowKind::ORDER_LABEL || row.kind == ViewRowKind::SCREENS_LABEL; }
// On a narrow (turned) screen the picture goes below 8 rows.
int view_visible_rows() { return screen::width() > 200 ? screen::rows() : 8; }
// The next zoom for this angle, larger (or smaller) than the one chosen, round again at the end.
// L / R held (L/R page turns on): R the next bigger zoom, L the next smaller,
// for as long as it is held, not stored (-1: none). hold_dir is the button
// that holds it (1 R, -1 L, 0 none). A page opened with a centre is put there.
int hold_zoom = -1;
int hold_dir = 0;
bool comic_center = false;
int center_x = 0, center_y = 0;
// Where the view was when a held zoom began: let go without moving (D-pad,
// A, B), it comes back exactly there (a smaller zoom's view is kept inside
// the page, so its middle can differ).
bool hold_home = false, hold_moved = false;
int hold_home_x = 0, hold_home_y = 0;

// The centre to reopen at when a held zoom changes to `to` (-1: ends).
bool hold_change_center(int to)
{
    if(hold_zoom < 0) {
        hold_home = comic_reader.view_center(hold_home_x, hold_home_y);
        hold_moved = false;
    }
    if(to < 0 && hold_home && !hold_moved) {
        center_x = hold_home_x;
        center_y = hold_home_y;
        return true;
    }
    return comic_reader.view_center(center_x, center_y);
}

// The next bigger zoom of the angle in use (the file lists each angle's
// zooms smallest first); -1 if this is the biggest.
int bigger_zoom(uint8_t zoom)
{
    const bool side = comic_sideways();
    for(int z = int(zoom) + 1; z < comic.zoom_count(); ++z)
        if(comic.zoom(z).sideways == side) return z;
    return -1;
}

// The next smaller zoom of the angle in use; -1 if this is the smallest.
int smaller_zoom(uint8_t zoom)
{
    const bool side = comic_sideways();
    for(int z = int(zoom) - 1; z >= 0; --z)
        if(comic.zoom(z).sideways == side) return z;
    return -1;
}

uint8_t next_zoom(uint8_t zoom, int delta)
{
    const bool side = comic_sideways();
    const int n = comic.zoom_count();
    for(int k = 1; k <= n; ++k) {
        const int z = ((int(zoom) + delta * k) % n + n) % n;
        if(comic.zoom(z).sideways == side) return uint8_t(z);
    }
    return zoom;
}

// The screens of the page on view at a zoom and order (for the View screen).
struct ViewLayout {
    ViewLayout() {}   // set up at run time (EWRAM .sbss takes no initial values)
    reader::gbcomic::View view;
    reader::comic_layout::Grid grids[reader::gbcomic::MAX_PICTURES];
    int tiles = 0;
};
int layout_view(ViewLayout& l, int page, int zoom, int order)
{
    l.tiles = reader::ComicReader::layout(comic, page, zoom, reader::gbcomic::Order(order), zoom_counts(zoom),
                                          l.view, l.grids);
    return l.tiles;
}

// The View screen: each zoom with the number of screens the page on view
// takes, and a picture of how the page is cut and read for the row the
// cursor is on.
void draw_view_diagram(uint8_t* pixels, int x, int y, int w, int h, const ViewLayout& l)
{
    const reader::gbcomic::View& v = l.view;
    // The page, scaled into the box.
    const int pw = v.page_w * h <= v.page_h * w ? v.page_w * h / v.page_h : w;
    const int ph = v.page_w * h <= v.page_h * w ? h : v.page_h * w / v.page_w;
    const int px = x + (w - pw) / 2, py = y + (h - ph) / 2;
    screen::fill(pixels, px - 1, py - 1, pw + 2, ph + 2, screen::BLACK);
    screen::fill(pixels, px, py, pw, ph, screen::WHITE);
    for(int t = 0; t < l.tiles; ++t) {
        const reader::comic_layout::Spot s = reader::comic_layout::spot_at(l.grids, v.picture_count, v.order, t);
        int b[4];
        reader::ComicReader::page_box(v, s.picture, l.grids[s.picture].xs[s.column], l.grids[s.picture].ys[s.row], b);
        const int tx = px + b[0] * pw / 4096, ty = py + b[1] * ph / 4096;
        const int tw = (b[2] - b[0]) * pw / 4096, th = (b[3] - b[1]) * ph / 4096;
        // Each screen's outline; its number in reading order where there is room.
        screen::fill(pixels, tx, ty, tw, 1, screen::GREY);
        screen::fill(pixels, tx, ty + th - 1, tw, 1, screen::GREY);
        screen::fill(pixels, tx, ty, 1, th, screen::GREY);
        screen::fill(pixels, tx + tw - 1, ty, 1, th, screen::GREY);
        if(t < 99 && tw >= 12 && th >= 10) {
            const bn::string<4> number = bn::to_string<4>(t + 1);
            screen::small_draw(pixels, tx + (tw - screen::small_width(number.data())) / 2, ty + th / 2 - 3,
                               number.data(), screen::BLACK);
        }
    }
}

void count_label(bn::string<40>& label, const char* name, int chosen, int shown)
{
    label = name;
    if(chosen) label += bn::to_string<4>(chosen);
    else { label += "Auto ("; label += bn::to_string<4>(shown); label += ")"; }
}

void draw_view_screen(uint8_t* pixels, const reader::ListNav& nav, int page)
{
    using namespace reader::gbcomic;
    screen::header(pixels, "View");
    const bool wide = screen::width() > 200;
    // Rows leave room for the picture: beside them, or below on a narrow screen.
    const int rows_w = wide ? 150 : screen::width();
    const int keep_w = screen::width();
    screen::set_width(rows_w);
    BN_DATA_EWRAM_BSS static ViewLayout l;
    ViewRow rows[VIEW_ROWS_MAX];
    const int count = view_rows(rows);
    const int zoom = comic_zoom();
    for(int slot = 0; slot < view_visible_rows() && nav.top + slot < count; ++slot) {
        const ViewRow row = rows[nav.top + slot];
        if(view_row_label(row)) {
            screen::divider(pixels, slot, row.kind == ViewRowKind::ORDER_LABEL ? "Reading order" : "Screens");
            continue;
        }
        bn::string<40> label;
        bn::string<16> info;
        bool chosen = false;
        if(row.kind == ViewRowKind::ZOOM) {
            char name[16];
            zoom_name(comic.zoom(row.value), name);
            label = name;
            chosen = row.value == zoom;
            if(layout_view(l, page, row.value, book_prefs.order)) {
                info = bn::to_string<4>(l.tiles);
                info += l.tiles == 1 ? " screen" : " screens";
            }
        } else if(row.kind == ViewRowKind::ORDER) {
            label = order_name(Order(row.value));
            chosen = row.value == book_prefs.order;
        } else if(row.kind == ViewRowKind::COLUMNS || row.kind == ViewRowKind::ROWS) {
            const bool across = row.kind == ViewRowKind::COLUMNS;
            const int counts = zoom_counts(zoom);
            const int chosen_count = across ? counts >> 4 : counts & 15;
            int shown = 1;
            if(layout_view(l, page, zoom, book_prefs.order))
                shown = across ? l.grids[0].columns : l.grids[0].rows;
            count_label(label, across ? "Columns: " : "Rows: ", chosen_count, shown);
        } else {
            label = "Skip empty: ";
            label += book_prefs.skip_empty ? "On" : "Off";
        }
        screen::row(pixels, slot, label.data(), nav.top + slot == nav.selected, false,
                    info.empty() ? nullptr : info.data());
        if(chosen)
            screen::tick(pixels, screen::TEXT_X + screen::text_width(label.data()) + 5,
                         screen::ROW_Y + slot * screen::ROW_H + 5, screen::BLACK);
    }
    screen::set_width(keep_w);
    // The picture: the zoom or order under the cursor with the other one chosen.
    const ViewRow at = rows[nav.selected];
    const int shown_zoom = at.kind == ViewRowKind::ZOOM ? at.value : zoom;
    const int order = at.kind == ViewRowKind::ORDER ? at.value : book_prefs.order;
    if(!layout_view(l, page, shown_zoom, order)) return;
    if(wide) draw_view_diagram(pixels, rows_w + 6, screen::ROW_Y + 4, keep_w - rows_w - 12, screen::height() - screen::ROW_Y - 12, l);
    else {
        const int top = screen::ROW_Y + view_visible_rows() * screen::ROW_H + 6;
        draw_view_diagram(pixels, 8, top, keep_w - 16, screen::height() - top - 6, l);
    }
}

// One About page as rows on the screen: long lines wrap on a narrow screen.
struct AboutRow {
    const char* text;
    int length;
    bool heading;
};
constexpr int ABOUT_ROWS_MAX = 40;

int about_layout(int about_page, int width, AboutRow (&rows)[ABOUT_ROWS_MAX])
{
    int count = 0;
    for(int i = 0; i < reader::ABOUT_LINES && count < ABOUT_ROWS_MAX; ++i) {
        const char* line = reader::about_lines[about_page][i];
        if(line[0] == '#' || !line[0]) {
            int length = 0;
            while(line[length]) ++length;
            rows[count++] = line[0] ? AboutRow{line + 1, length - 1, true} : AboutRow{line, 0, false};
            continue;
        }
        int starts[8], lengths[8];
        const int n = screen::wrap(line, width, starts, lengths, 8);
        for(int k = 0; k < n && count < ABOUT_ROWS_MAX; ++k)
            rows[count++] = AboutRow{line + starts[k], lengths[k], false};
    }
    return count;
}

void draw_list(uint8_t* pixels, const char* title, const reader::ListNav& nav, int count,
               const char* (*label)(int), bool (*folder)(int), unsigned skip)
{
    screen::header(pixels, title);
    for(int slot = 0; slot < screen::rows() && nav.top + slot < count; ++slot) {
        const int i = nav.top + slot;
        screen::row(pixels, slot, label(i), i == nav.selected, folder && folder(i), nullptr,
                    i == nav.selected ? skip : 0);
    }
}

bool no_folder(int) { return false; }

const char* settings_label(reader::SettingsItem item, bool shoulder, int goto_percent,
                           bool confirm_leave, bn::string<48>& out)
{
    using reader::SettingsItem;
    out.clear();
    switch(item) {
    case SettingsItem::GOTO:
        if(comic_open) { out = "Go to: page "; out += bn::to_string<6>(goto_percent); break; }
        out = "Go to: "; out += bn::to_string<4>(goto_percent); out += "%"; break;
    case SettingsItem::VIEW: {
        char name[16];
        reader::gbcomic::zoom_name(comic.zoom(comic_zoom()), name);
        out = "View: "; out += name; break;
    }
    case SettingsItem::PAGE_MAP:
        out = "Page map: "; out += global_settings.values.page_map ? "On" : "Off"; break;
    case SettingsItem::ANIMATIONS:
        out = "Animations"; break;
    case SettingsItem::LINE_SPACING:
        out = "Line spacing: "; out += bn::to_string<4>(settings.line_spacing); break;
    case SettingsItem::PARAGRAPH_GAP:
        out = "Paragraph gap: "; out += reader::paragraph_gap_name(settings.paragraph_gap); break;
    case SettingsItem::PAGE_TURN_KEYS:
        // The same setting; in a comic L / R held zoom in instead of turning pages.
        out = comic.is_open() ? "L/R zoom (held): " : "L/R page turns: ";
        out += shoulder ? "On" : "Off"; break;
    case SettingsItem::FILE_EXTENSIONS:
        out = "Show file extensions: ";
        out += global_settings.values.show_extensions ? "On" : "Off"; break;
    case SettingsItem::ANGLE:
        out = "Angle: "; out += reader::angle_name(settings.angle); break;
    case SettingsItem::BACK_TO_FILES:
        out = confirm_leave ? "Continue without saving?" : "Back to Files"; break;
    default:
        out = "About"; break;
    }
    return out.data();
}

struct ImportProgress {
    bn::palette_bitmap_bg_painter& painter;
};

void show_import_progress(void* opaque, int percent)
{
    auto& painter = static_cast<ImportProgress*>(opaque)->painter;
    painter.fill(screen::WHITE);
    uint8_t* pixels = page_pixels(painter);
    screen::header(pixels, "Import to /gbareader");
    screen::center_text(pixels, 64, "Importing...");
    bn::string<8> text = bn::to_string<4>(percent); text += "%";
    screen::center_text(pixels, 84, text.data());
    painter.flip_page_later();
    bn::core::update();
}

}

namespace rkey {

namespace {

volatile uint16_t& io16(unsigned address) { return *reinterpret_cast<volatile uint16_t*>(address); }

bool start_down() { return !(io16(0x04000130) & START); }

// IE's keypad bit, with IME off meanwhile (as libugba's IRQ_Enable).
void keypad_irq(bool on)
{
    volatile uint16_t& ime = io16(0x04000208);
    const uint16_t was = ime;
    ime = 0;
    if(on) io16(0x04000200) |= 1u << 12;
    else io16(0x04000200) &= uint16_t(~(1u << 12));
    ime = was;
}

// BIOS Stop: the CPU, the LCD and the sound stop until an interrupt enabled
// in IE that Stop answers (here the keypad's).
void bios_stop()
{
#if defined(__thumb__)
    asm volatile("swi 0x03" ::: "r0", "r1", "r2", "r3", "memory");
#else
    asm volatile("swi 0x030000" ::: "r0", "r1", "r2", "r3", "memory");
#endif
}

}

void sleep_until_start()
{
    // Butano's bn::core::sleep(START), except that START must stay down for
    // 1 s (60 frames) with the screen still off before it wakes: let go
    // sooner (also a bounce: up for an instant) and it stops again at once,
    // nothing redrawn. As Butano: START up first, one update, then Stop with
    // the core locked (no VBlank interrupt, so the key latch is not read).
    constexpr int HOLD_FRAMES = 60;
    bn::core::update();
    while(bn::keypad::held(bn::keypad::key_type::START)) bn::core::update();
    {
        bn::core_lock lock;   // its end wakes the display after a VBlank
        io16(0x04000000) |= 0x80;     // DISPCNT: forced blank
        io16(0x04000132) = 0x4008;    // KEYCNT: interrupt on START
        for(;;) {
            io16(0x04000202) = 1u << 12;   // IF: no old keypad request
            keypad_irq(true);
            // Stop ends on START down; anything else stops again.
            do bios_stop(); while(! start_down());
            keypad_irq(false);   // none while START is held
            const bn::timer held;
            const int hold_ticks = HOLD_FRAMES * bn::timers::ticks_per_frame();
            while(start_down() && held.elapsed_ticks() < hold_ticks) { }
            if(start_down()) break;
        }
        io16(0x04000132) = 0;
    }
    wake_guard_ref().start();
}

}

int main()
{
    bn::core::init();
    rkey::start_latch();   // every press counts, also while the loop is busy
    bn::palette_bitmap_bg_ptr background = bn::palette_bitmap_bg_ptr::create(palette_item);
    bn::palette_bitmap_bg_painter painter(background);
    painter.fill(0);
    painter.flip_page_later();
    bn::core::update(); // Commit the initial flip before the first Home redraw.

    bn::vector<bn::sprite_ptr, UI_SPRITE_CAPACITY> sprites;
    // Overlays drawn over book text (save..., Loading back..., L+R) use the white-box font.
    bn::sprite_font overlay_font(
            bn::sprite_items::ui_small_font_box, bn::utf8_characters_map_ref(),
            reader::ui_small_font_character_widths);
    bn::sprite_text_generator save_ui(overlay_font);
    save_ui.set_palette_item(bn::sprite_items::ui_small_font_box.palette_item());
    bn::vector<bn::sprite_ptr, SAVE_OVERLAY_SPRITE_CAPACITY> save_sprites;


    settings = reader::default_settings();
    bool storage_ok = reader::storage_init();
    const auto globals_loaded = storage_ok ? global_settings.load() : reader::GlobalLoadResult::ERROR;
    Scene scene = Scene::LIBRARY;
    reader::ListNav home{};
    home.selected = storage_ok ? reader::remembered_library_selection(
            global_settings.values.last_book, reader::library_count(), reader::library_name) : 0;
    reader::list_show(home, reader::library_count(), screen::ROWS);
    reader::ListNav settings_nav{};
    reader::SettingsItem settings_items[reader::SETTINGS_MAX_ROWS];
    int settings_count = 0;
    reader::Settings settings_before = settings;
    // This book's own settings when Settings opened.
    reader::BookPrefs prefs_before{};
    // Comic on screen: the page, and whether it has to be opened (from its
    // first screen, or its last going back).
    int comic_page = 0;
    bool comic_reopen = true, comic_at_bottom = false, comic_failed = false;
    // The View screen: zoom, reading order, screens, Skip empty.
    reader::ListNav view_nav{};
    // The Animations screen (comics): how the D-pad and A / B move the screen.
    reader::ListNav animations_nav{};
    // The Angle screen (this book): Normal, 90° right, 90° left, Upside down.
    reader::ListNav angle_nav{};
    // Start: held with the D-pad it moves the screen; alone it saves on release.
    bool start_down = false, start_used = false;
    reader::KeyRepeat nudge_up, nudge_down, nudge_left, nudge_right;
    // Comics, D-pad scrolling: frames held, and what is left of the speed after letting go (Smooth).
    int pad_frames = 0, glide_x = 0, glide_y = 0;
    // A / B pressed while the D-pad was held: the D-pad does nothing until it is let go.
    bool pad_blocked = false;
    int about_scroll = 0, about_count = 0;
    int goto_percent = 0;
    int goto_before = 0;
    int count_refresh_frames = 0;
    int about_page = 0;
    // Back to Files asks first when the page shown is not the saved one.
    uint32_t saved_offset = 0;
    bool confirm_leave = false;
    // The last L/R choice is restored at launch and saved whenever it changes.
    reader::ReaderHold reader_hold{};
    reader_hold.shoulder_page_turns = global_settings.values.shoulder_page_turns;
    bool redraw_ui = true;
    bool redraw_page = false;
    const char* open_name = nullptr;
    const reader::ByteSource* active_source = &file;
    const char* message = globals_loaded == reader::GlobalLoadResult::ERROR && storage_ok ?
            "Settings load failed" :
            globals_loaded == reader::GlobalLoadResult::RECOVERED ? "Settings recovered" : nullptr;
    int message_frames = message ? MESSAGE_FRAMES : 0;
    reader::SaveMessageTimer save_message_timer{};
    // B presses waiting for older pages (Loading back...).
    int pending_back = 0;
    ListKeys list_keys;
    reader::KeyRepeat goto_left, goto_right;
    reader::Marquee marquee;
    unsigned frame = 0;
    // Import browser: one remembered cursor per folder level.
    reader::ListNav import_nav[IMPORT_DEPTH]{};
    int import_depth = 0;
    bool import_ok = false;
    reader::ListNav confirm_nav{};
    // Buffers below live in EWRAM: the IWRAM stack is small.
    BN_DATA_EWRAM_BSS static char import_source[reader::BROWSE_PATH_MAX];
    const char* import_name = nullptr;

    auto flash = [&](const char* text) {
        message = text;
        message_frames = MESSAGE_FRAMES;
        redraw_ui = true;
    };
    // A turned book turns its page, Settings, About and the D-pad with it.
    auto apply_view = [&]() {
        const bool turned = open_name && settings.angle != reader::Angle::NORMAL &&
                (scene == Scene::READER || scene == Scene::SETTINGS || scene == Scene::ABOUT ||
                 scene == Scene::VIEW || scene == Scene::ANIMATIONS || scene == Scene::ANGLE);
        const reader::Angle angle = turned ? settings.angle : reader::Angle::NORMAL;
        rkey::set_angle(angle);
        screen::set_canvas(reader::turned_width(angle), reader::turned_height(angle));
        return turned;
    };
    auto open_settings = [&](bool book_open) {
        settings_count = reader::settings_items(book_open, settings_items, comic_open);
        settings_nav = {};
        confirm_leave = false;
        prefs_before = book_prefs;
        if(!book_open) {
            settings.line_spacing = global_settings.values.line_spacing;
            settings.paragraph_gap = global_settings.values.paragraph_gap;
        }
        settings_before = settings;
        list_keys.reset();
        scene = Scene::SETTINGS;
        redraw_ui = true;
    };
    // A comic page's colours into screen entries 8 onwards (shown with the next flip).
    auto set_comic_colors = [&](const uint16_t* colors) {
        comic_colors = initial_colors;
        for(int i = 0; i < reader::gbcomic::COLORS; ++i) {
            const uint16_t c = colors[i];
            comic_colors.colors[reader::gbcomic::FIRST_COLOR + i] = bn::color(c & 31, (c >> 5) & 31, (c >> 10) & 31);
        }
        bn::bg_palette_ptr colors_on_screen = background.palette();
        colors_on_screen.set_colors(bn::span<const bn::color>(comic_colors.colors));
    };
    auto toggle_shoulder_turns = [&]() {
        reader_hold.shoulder_page_turns = ! reader_hold.shoulder_page_turns;
        global_settings.values.shoulder_page_turns = reader_hold.shoulder_page_turns;
        const bool mode_saved = global_settings.save();
        show_overlay(save_ui, sprites, !mode_saved ? "Settings save failed" :
                     reader_hold.shoulder_page_turns ? "L+R: On" : "L+R: Off", -64);
        reader_hold.mode_message_frames = 60; // One second of application frames, non-blocking.
    };
    auto close_comic = [&]() {
        hold_zoom = -1;
        hold_dir = 0;
        comic_center = false;
        comic.close();
        reset_epub();
        comic_open = false;
    };
    // A .gbcomic: its pages, and the page saved last. False (status says why) if it cannot be read.
    auto open_comic = [&](const char* name, const char*& status) {
        status = nullptr;
        if(!comic.open(file, book_memory, reader::gbcomic::Book::ARENA_BYTES)) {
            status = reader::gbcomic::open_error_string(comic.error());
            close_comic();
            return false;
        }
        comic_reader.set_memory(book_memory + reader::gbcomic::Book::ARENA_BYTES,
                                BOOK_MEMORY_BYTES - reader::gbcomic::Book::ARENA_BYTES);
        comic_open = true;
        open_name = name;
        book_prefs = file.prefs();
        book_prefs.comic = true;
        // Zooms, order and angle the file has (a new book: the first zoom larger than Fit width).
        if(!book_prefs.comic_known || !zoom_for(book_prefs.zoom, false)) book_prefs.zoom = first_zoom(false);
        if(!book_prefs.comic_known || !zoom_for(book_prefs.side_zoom, true)) book_prefs.side_zoom = first_zoom(true);
        book_prefs.comic_known = true;
        if(book_prefs.order >= reader::gbcomic::ORDER_COUNT) book_prefs.order = 0;
        if(!comic_angle_ok(book_prefs.angle))
            book_prefs.angle = comic.has_upright() ? reader::Angle::NORMAL : reader::Angle::RIGHT;
        settings.angle = book_prefs.angle;
        reader::clear_footer(loaded_state);
        comic_page = file.saved_footer(loaded_state) &&
                loaded_state.byte_offset < uint32_t(comic.page_count()) ? int(loaded_state.byte_offset) : 0;
        comic_reopen = true;
        comic_at_bottom = false;
        saved_offset = uint32_t(comic_page);
        reader::clear_footer(saved_state);
        saved_state.byte_offset = saved_offset;
        saved_state.settings = settings;
        pending_back = false;
        reader::cancel_save_message(save_message_timer);
        save_sprites.clear();
        sprites.clear();
        reader::remember_global_book(global_settings.values, open_name);
        if(!global_settings.save()) {
            show_overlay(save_ui, save_sprites, "Settings save failed");
            reader::start_save_message(save_message_timer);
        }
        scene = Scene::READER;
        message = nullptr; message_frames = 0;
        return true;
    };
    auto enter_import = [&](const char* folder) {
        import_ok = reader::browse_open(folder);
        reader::list_show(import_nav[import_depth], reader::browse_count(), screen::ROWS);
        marquee.reset();
        list_keys.reset();
        scene = Scene::IMPORT;
        redraw_ui = true;
    };

    while(true) {
        const bn::timer frame_timer;
        ++frame;
        rkey::next_frame();
        if(scene == Scene::LIBRARY && rkey::lr_chord()) {
            // Book list, L+R: sleep until START, then the same list; this
            // frame's keys and the waking START do nothing else.
            rkey::sleep_until_start();
            rkey::frame_ref() = rkey::Frame{};
        }
        bool book_open = open_name != nullptr;
        apply_view();
        // Reader hold sampling: keep edge history in every scene.
        if(reader_hold.mode_message_frames && !--reader_hold.mode_message_frames) sprites.clear();
        const unsigned reader_action = sample_reader_hold(reader_hold, scene);
        // End reader hold sampling.
        if(scene != Scene::READER && message_frames && !--message_frames) {
            message = nullptr;
            redraw_ui = true;
        }
        // Reading: Start saves when let go, unless the D-pad was used meanwhile.
        bool start_save = false;
        if(scene != Scene::READER) {
            start_down = false;
        } else {
            if(rkey::start_pressed()) { start_down = true; start_used = false; }
            if(start_down && !rkey::start_held()) {
                start_down = false;
                start_save = !start_used;
            }
            if(start_down && (rkey::up_pressed() || rkey::down_pressed() || rkey::left_pressed() ||
                              rkey::right_pressed())) start_used = true;
        }
        if(scene == Scene::LIBRARY) {
            const int count = storage_ok ? reader::library_count() : 0;
            if(list_keys.update(home, count, true)) { marquee.reset(); redraw_ui = true; }
            if(rkey::select_pressed()) {
                open_settings(false);
            } else if(rkey::start_pressed()) {
                if(!storage_ok) flash("No SD card");
                else { import_depth = 0; import_nav[0] = {}; enter_import("/"); }
            } else if(rkey::a_pressed() && count) {
                const char* library_status = nullptr;
                BN_DATA_EWRAM_BSS static char path[reader::LIBRARY_PATH_MAX];
                if(!reader::library_path(home.selected, path) || !file.open_read_only(path)) {
                    flash("Book open failed");
                } else if(reader::gbcomic_book_name(path)) {
                    const char* status;
                    if(!open_comic(reader::library_name(home.selected), status)) { file.close(); flash(status); }
                } else {
                  open_name = reader::library_name(home.selected);
                active_source = &file;
                if(epub_name(open_name)) {
                    if(epub.open(file)) active_source = &epub;
                    else library_status = reader::epub_error_string(epub.error());
                }
                book_prefs = file.prefs();
                book_prefs.comic = false;
                settings.angle = book_prefs.angle;
                reader::clear_footer(loaded_state);
                const bool footer_loaded = file.saved_footer(loaded_state);
                const bool prepare_cache = active_source == &epub && epub.needs_cache_persistence();
                struct SaveContext {
                    const reader::ByteSource* cache;
                    bn::sprite_text_generator& ui;
                    bn::vector<bn::sprite_ptr, SAVE_OVERLAY_SPRITE_CAPACITY>& sprites;
                    bool prepare_cache;
                } save_context{active_source == &epub ? active_source : nullptr,
                               save_ui, save_sprites, prepare_cache};
                reader::cancel_save_message(save_message_timer);
                save_sprites.clear();
                auto opening_save = [](void* opaque, const reader::TxtSaveFooter& state) {
                    auto& context = *static_cast<SaveContext*>(opaque);
                    show_overlay(context.ui, context.sprites,
                                 context.prepare_cache ? "Preparing cache..." : "save...");
                    bn::core::update();
                    return file.save_footer(state, context.cache);
                };
                settings.line_spacing = global_settings.values.line_spacing;
                settings.paragraph_gap = global_settings.values.paragraph_gap;
                auto opened = library_status ? reader::OpenResult::FAILED : reader::open_document_page(
                        *active_source, footer_loaded ? &loaded_state : nullptr, settings, glyph_width,
                        history, page, history_rebuild, prepare_cache, opening_save, &save_context);
                // Preserve the existing one-time cache switch after a verified save.
                if(opened == reader::OpenResult::SAVED && prepare_cache) {
                    epub.close();
                    if(!epub.open(file)) opened = reader::OpenResult::FAILED;
                }
                save_sprites.clear();
                if(opened != reader::OpenResult::FAILED) {
                    pending_back = false;
                    restart_page_count();
                    saved_offset = page.start_offset;
                    reader::set_footer(saved_state, page.start_offset, settings, history, history_rebuild);
                    reader::remember_global_book(global_settings.values, open_name);
                    const bool globals_saved = global_settings.save();
                    if(opened == reader::OpenResult::SAVE_FAILED || !globals_saved) {
                        // Both independently pending states remain usable in RAM.
                        show_overlay(save_ui, save_sprites, reader::save_status_message(
                                opened != reader::OpenResult::SAVE_FAILED, globals_saved));
                        reader::start_save_message(save_message_timer);
                    }
                    scene = Scene::READER;
                    message = nullptr; message_frames = 0;
                    sprites.clear();
                    redraw_page = true;
                } else {
                    if(! library_status) library_status = epub_name(open_name) ?
                            reader::epub_error_string(epub.error()) : "Book read failed";
                    epub.close();
                    file.close();
                    open_name = nullptr;
                    settings.angle = reader::Angle::NORMAL;
                    flash(library_status);
                }
                }
            }
        } else if(scene == Scene::READER && comic_open) {
            const int count = comic.page_count();
            // A / B: screens and pages. The D-pad moves around the picture.
            const bool forward_pressed = rkey::a_pressed();
            const bool back_pressed = rkey::b_pressed();
            // L / R (when on) held: R the next bigger zoom, L the next smaller, around
            // the middle of the screen, for as long as it is held (both held cancel
            // out); let go, the book's own zoom again. No such zoom: nothing happens.
            const int want = !reader_hold.shoulder_page_turns ? 0 : int(rkey::r_held()) - int(rkey::l_held());
            if(want != hold_dir && !comic_reopen && !start_down) {
                const int to = want > 0 ? bigger_zoom(comic_zoom()) : want < 0 ? smaller_zoom(comic_zoom()) : -1;
                hold_dir = want;
                if(to != hold_zoom) {
                    comic_center = hold_change_center(to);
                    hold_zoom = to;
                    comic_reopen = true;
                    comic_at_bottom = false;
                    glide_x = glide_y = 0;
                }
            }
            const bool any_pad = rkey::up_held() || rkey::down_held() || rkey::left_held() || rkey::right_held();
            // A / B take over: the D-pad held then would cut the slide short and scroll on.
            if(forward_pressed || back_pressed) pad_blocked = any_pad;
            if(!any_pad) pad_blocked = false;
            const bool pad_held = !start_down && !pad_blocked && any_pad;
            if(hold_zoom >= 0 && (pad_held || forward_pressed || back_pressed)) hold_moved = true;
            if(!start_down && !pad_blocked && (pad_held || rkey::up_pressed() || rkey::down_pressed() ||
                                               rkey::left_pressed() || rkey::right_pressed())) {
                const int ux = (rkey::right_held() ? 1 : 0) - (rkey::left_held() ? 1 : 0);
                const int uy = (rkey::down_held() ? 1 : 0) - (rkey::up_held() ? 1 : 0);
                if(global_settings.values.slide_pad == reader::Slide::STATIC) {
                    const int u = nudge_up.update(rkey::up_held(), rkey::up_pressed());
                    const int d = nudge_down.update(rkey::down_held(), rkey::down_pressed());
                    const int l = nudge_left.update(rkey::left_held(), rkey::left_pressed());
                    const int r = nudge_right.update(rkey::right_held(), rkey::right_pressed());
                    comic_reader.move_by((r ? 16 : 0) - (l ? 16 : 0), (d ? 16 : 0) - (u ? 16 : 0));
                } else if(pad_held) {
                    // Slide: an even speed. Smooth: faster the longer it is held, and it glides to a stop.
                    ++pad_frames;
                    int speed = 4;
                    if(global_settings.values.slide_pad == reader::Slide::SMOOTH) {
                        speed = 2 + 2 * (pad_frames / 8);
                        if(speed > 8) speed = 8;
                    }
                    glide_x = ux * speed;
                    glide_y = uy * speed;
                    comic_reader.move_by(glide_x, glide_y);
                }
            }
            if(forward_pressed) {
                glide_x = glide_y = 0;
                const auto step = comic_reader.page_open() ? comic_reader.forward(global_settings.values.slide_ab, global_settings.values.slide_row) :
                                  reader::ComicReader::Step::NEXT_PAGE;
                if(step == reader::ComicReader::Step::NEXT_PAGE && comic_page + 1 < count) {
                    ++comic_page;
                    comic_reopen = true;
                    comic_at_bottom = false;
                }
            } else if(back_pressed) {
                glide_x = glide_y = 0;
                const auto step = comic_reader.page_open() ? comic_reader.back(global_settings.values.slide_ab, global_settings.values.slide_row) :
                                  reader::ComicReader::Step::PREVIOUS_PAGE;
                if(step == reader::ComicReader::Step::PREVIOUS_PAGE && comic_page > 0) {
                    --comic_page;
                    comic_reopen = true;
                    comic_at_bottom = true;
                }
            } else if(start_save) {
                reader::clear_footer(work_state);
                work_state.byte_offset = uint32_t(comic_page);
                work_state.settings = settings;
                reader::cancel_save_message(save_message_timer);
                show_saving_overlay(save_ui, save_sprites);
                bn::core::update();
                const bool saved = file.save_footer(work_state, nullptr);
                if(saved) { saved_offset = uint32_t(comic_page); saved_state = work_state; }
                const bool globals_saved = global_settings.save();
                show_overlay(save_ui, save_sprites, reader::save_status_message(saved, globals_saved));
                reader::start_save_message(save_message_timer);
            } else if(rkey::select_pressed()) {
                goto_before = goto_percent = comic_page + 1;
                reader::cancel_save_message(save_message_timer);
                save_sprites.clear();
                sprites.clear();
                // Settings end a held zoom: back to the book's own, at the same place.
                if(hold_zoom >= 0) {
                    comic_center = hold_change_center(-1);
                    hold_zoom = -1;
                    comic_reopen = true;
                    comic_at_bottom = false;
                }
                hold_dir = 0;
                open_settings(true);
            }
            if(!pad_held) {
                pad_frames = 0;
                // Smooth: the view glides on a little after the D-pad is let go,
                // slowing down in even steps (8: 6 6 4 4 2 2; an odd step across the
                // screen is a slow copy).
                if(global_settings.values.slide_pad == reader::Slide::SMOOTH && (glide_x || glide_y)) {
                    glide_x -= glide_x > 0 ? 1 : glide_x < 0 ? -1 : 0;
                    glide_y -= glide_y > 0 ? 1 : glide_y < 0 ? -1 : 0;
                    comic_reader.move_by(glide_x - glide_x % 2, glide_y - glide_y % 2);
                } else glide_x = glide_y = 0;
            }
            if(scene == Scene::READER) {
                if(comic_reopen) {
                    comic_reopen = false;
                    const int zoom = hold_zoom >= 0 ? hold_zoom : comic_zoom();
                    comic_failed = !comic_reader.open_page(comic, comic_page, zoom, reader::gbcomic::Order(book_prefs.order),
                                                           zoom_counts(zoom), book_prefs.skip_empty, global_settings.values.page_map,
                                                           comic_at_bottom);
                    if(comic_center && !comic_failed) comic_reader.center_on(center_x, center_y);
                    comic_center = false;
                    sprites.clear();
                    if(comic_failed) {
                        painter.fill(screen::WHITE);
                        show_overlay(save_ui, sprites, "Page read failed");
                        painter.flip_page_later();
                    }
                }
                BN_DATA_EWRAM_BSS static uint16_t colors[reader::gbcomic::COLORS];
                bool new_colors = false, failed = false;
                uint16_t* hidden = painter.page().data();
                const auto* shown = reinterpret_cast<const uint16_t*>(uintptr_t(hidden) ^ 0xA000u);
                if(!comic_failed && comic_reader.draw(comic, hidden, shown, settings.angle, colors, new_colors, failed)) {
                    if(new_colors) set_comic_colors(colors);
                    painter.flip_page_later();
                }
                if(failed && !comic_failed) {
                    comic_failed = true;
                    painter.fill(screen::WHITE);
                    sprites.clear();
                    show_overlay(save_ui, sprites, "Page read failed");
                    painter.flip_page_later();
                }
                // What comes next, while this frame has time left: only as many packed
                // bytes as the reading speed (learned as it goes) gets through in it.
                // Standing still, at least one chunk a frame even if it is bigger than
                // that: else a chunk bigger than a frame's worth is never loaded ahead
                // and the next slide has to read it all on the way (a frame late
                // while nothing moves does not show).
                static uint32_t bytes_per_frame = 4096;
                constexpr int frame_ticks = bn::timers::ticks_per_frame();
                bool first = true;
                while(!comic_failed) {
                    const int now = frame_timer.elapsed_ticks();
                    const int left = frame_ticks * 9 / 10 - now;
                    if(left <= 0 || now > COMIC_PREFETCH_TICKS) break;
                    const uint32_t before = comic.bytes_read();
                    const bool more = comic_reader.prefetch(comic, 2, uint32_t(left) * bytes_per_frame / frame_ticks);
                    if(!more && first && comic.bytes_read() == before && comic_reader.waiting())
                        comic_reader.prefetch(comic, 1);
                    first = false;
                    const uint32_t read = comic.bytes_read() - before;
                    const int spent = frame_timer.elapsed_ticks() - now;
                    if(read >= 512 && spent > 0)
                        bytes_per_frame = (bytes_per_frame * 3 + read * uint32_t(frame_ticks) / uint32_t(spent)) / 4;
                    comic_reader.set_read_speed(bytes_per_frame);
                    if(!more) break;
                }
            }
        } else if(scene == Scene::READER) {
            // Few pages left for Back: load the window before the oldest one ahead of time.
            if(history_rebuild.state == reader::HistoryRebuildState::IDLE &&
               history.count < reader::HISTORY_PREFETCH_PAGES) {
                const uint32_t older_end = reader::history_rebuild_anchor(history, page.start_offset);
                if(older_end > 0) reader::begin_history_rebuild(older_end, history_rebuild);
            }
            reader::Page& next = next_page_buffer;
            const bool forward_pressed = rkey::right_pressed() || rkey::a_pressed() ||
                                         (reader_hold.shoulder_page_turns && rkey::r_pressed());
            const bool back_pressed = rkey::left_pressed() || rkey::b_pressed() ||
                                      (reader_hold.shoulder_page_turns && rkey::l_pressed());
            if(start_down) {
                // (A text page has nothing to move while Start is held.)
            } else if(reader_action == 1) {
                toggle_shoulder_turns();
            } else if(forward_pressed) {
                if(pending_back) save_sprites.clear();
                pending_back = false;
                if(reader::next_page(*active_source, settings, glyph_width, history, page, next)) {
                    page = next;
                    ++page_turns;
                    redraw_page = true;
                }
            } else if(back_pressed) {
                if(reader::previous_page(*active_source, settings, glyph_width, history, next)) {
                    page = next;
                    --page_turns;
                    redraw_page = true;
                } else if(history_rebuild.state == reader::HistoryRebuildState::BUILDING ||
                          history_rebuild.state == reader::HistoryRebuildState::READY) {
                    ++pending_back;
                    reader::cancel_save_message(save_message_timer);
                    show_overlay(save_ui, save_sprites, "Loading back...");
                }
            } else if(pending_back && (history.count > 0 ||
                                       (history_rebuild.state != reader::HistoryRebuildState::BUILDING &&
                                        history_rebuild.state != reader::HistoryRebuildState::READY))) {
                // B pressed while older pages were loading: a page back each frame once they
                // are there (none left to load: the book's start, the rest is dropped).
                --pending_back;
                if(reader::previous_page(*active_source, settings, glyph_width, history, next)) {
                    page = next;
                    --page_turns;
                    redraw_page = true;
                } else {
                    pending_back = 0;
                }
                if(!pending_back) save_sprites.clear();
            } else if(start_save) {
                pending_back = false;
                reader::set_footer(work_state, page.start_offset, settings, history, history_rebuild);
                reader::cancel_save_message(save_message_timer);
                show_saving_overlay(save_ui, save_sprites);
                bn::core::update();
                const bool saved = file.save_footer(
                        work_state, active_source == &epub ? active_source : nullptr);
                if(saved) { saved_offset = page.start_offset; saved_state = work_state; }
                const bool globals_saved = global_settings.save();
                show_overlay(save_ui, save_sprites, reader::save_status_message(saved, globals_saved));
                reader::start_save_message(save_message_timer);
            } else if(rkey::select_pressed()) {
                pending_back = false;
                goto_before = goto_percent = reader::page_percent(page, active_source->size());
                reader::cancel_save_message(save_message_timer);
                save_sprites.clear();
                open_settings(true);
            }

            const bool idle_frame = !rkey::up_pressed() && !rkey::down_pressed() &&
                                    !rkey::left_pressed() && !rkey::right_pressed() &&
                                    !rkey::a_pressed() && !rkey::b_pressed() &&
                                    !rkey::start_pressed() && !rkey::select_pressed() &&
                                    !rkey::l_pressed() && !rkey::r_pressed();
            // Background page layouts for part of each idle frame; Back history first.
            if(scene == Scene::READER && idle_frame) {
                const bn::timer work_timer;
                do {
                    if(history_rebuild.state == reader::HistoryRebuildState::BUILDING)
                        reader::step_history_rebuild(
                                *active_source, settings, glyph_width, history_rebuild);
                    else if(page_count.state == reader::HistoryRebuildState::BUILDING)
                        reader::step_page_count(*active_source, settings, glyph_width, page_count);
                    else
                        break;
                } while(work_timer.elapsed_ticks() < BACKGROUND_WORK_TICKS);
            }
            if(scene == Scene::READER && history_rebuild.state == reader::HistoryRebuildState::READY) {
                const bool merged = history.count == 0 ?
                        page.start_offset == history_rebuild.anchor &&
                        reader::adopt_rebuilt_history(history_rebuild, history) :
                        reader::prepend_rebuilt_history(history_rebuild, history);
                // Pages moved on since this window started: drop it and load a fresh one.
                // (B presses waiting go back from the next frame on.)
                if(!merged) history_rebuild = {};
            }
            if(history_rebuild.state == reader::HistoryRebuildState::FAILED && pending_back) {
                pending_back = false;
                save_sprites.clear();
            }
            if(scene == Scene::READER && active_source == &epub &&
               epub.error() != reader::EpubError::NONE) {
                pending_back = false;
                history_rebuild = {};
                reader::cancel_save_message(save_message_timer);
                save_sprites.clear();
                const char* error = reader::epub_error_string(epub.error());
                epub.close(); file.close(); open_name = nullptr;
                settings.angle = reader::Angle::NORMAL;
                scene = Scene::LIBRARY;
                flash(error);
            }
        } else if(scene == Scene::SETTINGS) {
            // Keep counting pages while Settings is open, then show the number.
            if(book_open && !comic_open && reader::same_settings(settings_before, settings) &&
               page_count.state == reader::HistoryRebuildState::BUILDING) {
                const bn::timer work_timer;
                while(page_count.state == reader::HistoryRebuildState::BUILDING &&
                      work_timer.elapsed_ticks() < BACKGROUND_WORK_TICKS)
                    reader::step_page_count(*active_source, settings, glyph_width, page_count);
                // Refresh the estimated number twice a second, the exact one when done.
                if(page_count.state != reader::HistoryRebuildState::BUILDING || ++count_refresh_frames >= 30) {
                    count_refresh_frames = 0;
                    redraw_ui = true;
                }
            }
            if(list_keys.update(settings_nav, settings_count, false)) {
                // Step over the "This book" divider, and keep it in sight above its rows.
                settings_nav.selected = reader::settings_skip_divider(
                        settings_items, settings_count, settings_nav.selected, rkey::up_held() ? -1 : 1);
                reader::list_show(settings_nav, settings_count, screen::rows());
                if(settings_nav.top == settings_nav.selected && settings_nav.top > 0 &&
                   !reader::selectable(settings_items[settings_nav.top - 1])) --settings_nav.top;
                confirm_leave = false;
                redraw_ui = true;
            }
            const reader::SettingsItem item = settings_items[settings_nav.selected];
            int delta = rkey::left_pressed() ? -1 : rkey::right_pressed() ? 1 : 0;
            bool close = rkey::b_pressed() || rkey::select_pressed() ||
                         rkey::start_pressed();
            if(confirm_leave && rkey::b_pressed()) {
                // B takes the question back; Settings stays open.
                confirm_leave = false;
                close = false;
                redraw_ui = true;
            }
            bool go_now = false, back_to_files = false;
            if(item == reader::SettingsItem::GOTO) {
                const int l = goto_left.update(rkey::left_held(), rkey::left_pressed());
                const int r = goto_right.update(rkey::right_held(), rkey::right_pressed());
                delta = l ? -1 : r ? 1 : 0;
                if(rkey::l_pressed()) delta = -10;
                if(rkey::r_pressed()) delta = 10;
                if(delta) {
                    // A comic goes to a page number, a text to a percentage.
                    const int lowest = comic_open ? 1 : 0, highest = comic_open ? comic.page_count() : 100;
                    goto_percent += delta;
                    if(goto_percent < lowest) goto_percent = lowest;
                    if(goto_percent > highest) goto_percent = highest;
                    redraw_ui = true;
                }
                // A on Go to jumps now; B/Select/Start apply everything and return as before.
                go_now = rkey::a_pressed();
            } else if(item == reader::SettingsItem::FILE_EXTENSIONS) {
                bool& on = global_settings.values.show_extensions;
                if(rkey::a_pressed()) delta = on ? -1 : 1;
                if(delta) {
                    on = delta > 0;
                    redraw_ui = true;
                }
            } else if(item == reader::SettingsItem::PAGE_TURN_KEYS) {
                if(rkey::a_pressed()) delta = reader_hold.shoulder_page_turns ? -1 : 1;
                if(delta) {
                    reader_hold.shoulder_page_turns = delta > 0;
                    redraw_ui = true;
                }
            } else if(item == reader::SettingsItem::LINE_SPACING ||
                      item == reader::SettingsItem::PARAGRAPH_GAP) {
                const auto field = item == reader::SettingsItem::LINE_SPACING ?
                        reader::SettingField::LINE_SPACING : reader::SettingField::PARAGRAPH_GAP;
                if(rkey::a_pressed()) {
                    // A steps forward and wraps back to the first value.
                    const reader::Settings before = settings;
                    reader::adjust_setting(settings, field, 1);
                    if(reader::same_settings(before, settings)) reader::adjust_setting(settings, field, -100);
                    redraw_ui = true;
                } else if(delta) {
                    reader::adjust_setting(settings, field, delta);
                    redraw_ui = true;
                }
            } else if(item == reader::SettingsItem::BACK_TO_FILES) {
                if(rkey::a_pressed()) {
                    const uint32_t shown = comic_open ? uint32_t(comic_page) : page.start_offset;
                    if(confirm_leave || shown == saved_offset) back_to_files = true;
                    else { confirm_leave = true; redraw_ui = true; }
                }
            } else if(item == reader::SettingsItem::VIEW) {
                if(rkey::a_pressed()) {
                    view_nav = {};
                    ViewRow rows[VIEW_ROWS_MAX];
                    const int n = view_rows(rows);
                    for(int i = 0; i < n; ++i)
                        if(rows[i].kind == ViewRowKind::ZOOM && rows[i].value == comic_zoom()) view_nav.selected = i;
                    reader::list_show(view_nav, n, view_visible_rows());
                    list_keys.reset();
                    scene = Scene::VIEW;
                    redraw_ui = true;
                    delta = 0;
                }
                if(delta) {
                    choose_zoom(next_zoom(comic_zoom(), delta));
                    redraw_ui = true;
                }
            } else if(item == reader::SettingsItem::PAGE_MAP) {
                if(rkey::a_pressed() || delta) {
                    global_settings.values.page_map = !global_settings.values.page_map;
                    redraw_ui = true;
                }
            } else if(item == reader::SettingsItem::ANIMATIONS) {
                if(rkey::a_pressed() || rkey::right_pressed()) {
                    animations_nav = {};
                    list_keys.reset();
                    scene = Scene::ANIMATIONS;
                    redraw_ui = true;
                }
            } else if(item == reader::SettingsItem::ANGLE) {
                if(rkey::a_pressed()) {
                    // A: the Angle screen, the cursor on the angle chosen.
                    angle_nav = {};
                    angle_nav.selected = int(settings.angle);
                    list_keys.reset();
                    scene = Scene::ANGLE;
                    redraw_ui = true;
                } else if(delta) {
                    // A comic offers the angles it has screens for.
                    do {
                        settings.angle = reader::Angle(
                                (int(settings.angle) + reader::ANGLE_COUNT + delta) % reader::ANGLE_COUNT);
                    } while(comic_open && !comic_angle_ok(settings.angle));
                    // The screen and D-pad turn now; the list gets more or fewer rows.
                    apply_view();
                    reader::list_show(settings_nav, settings_count, screen::rows());
                    list_keys.reset();
                    redraw_ui = true;
                }
            } else if(rkey::a_pressed()) {
                about_page = 0;
                about_scroll = 0;
                scene = Scene::ABOUT;
                redraw_ui = true;
            }
            if(go_now || close || back_to_files) {
                if(comic_open && !back_to_files) {
                    if(goto_percent != goto_before) {
                        comic_page = goto_percent - 1;
                        comic_at_bottom = false;
                    }
                    // Settings covered the page (and the memory kept around it): draw it
                    // again, from its first screen after a change.
                    reader::BookPrefs now = book_prefs;
                    now.angle = settings.angle;
                    comic_reader.forget_region();
                    if(goto_percent != goto_before || !reader::same_book_prefs(now, prefs_before) || comic_failed) {
                        comic_reopen = true;
                        comic_failed = false;
                    }
                } else if(book_open && !back_to_files) {
                    uint32_t resume_offset = page.start_offset;
                    const bool jump = goto_percent != goto_before &&
                            reader::percent_offset(*active_source, goto_percent, resume_offset);
                    const bool relayout = !reader::same_settings(settings_before, settings);
                    if(jump || relayout) {
                        reader::open_page_at(*active_source, resume_offset, settings, glyph_width, history, page);
                        reader::begin_history_rebuild(resume_offset, history_rebuild);
                        if(relayout) restart_page_count();
                        else retarget_page_count();
                    }
                }
                pending_back = false;
                save_sprites.clear();
                global_settings.values.line_spacing = settings.line_spacing;
                global_settings.values.paragraph_gap = settings.paragraph_gap;
                global_settings.values.shoulder_page_turns = reader_hold.shoulder_page_turns;
                bool globals_saved = global_settings.save();
                if(book_open) {
                    // This book's settings go into its .sav with the position saved last,
                    // so leaving Settings never saves a newer page.
                    book_prefs.angle = settings.angle;
                    if(!reader::same_book_prefs(book_prefs, prefs_before)) {
                        file.set_prefs(book_prefs);
                        if(!file.save_footer(saved_state, nullptr)) globals_saved = false;
                        prefs_before = book_prefs;
                    }
                }
                if(back_to_files) {
                    history_rebuild = {};
                    settings.angle = reader::Angle::NORMAL;
                    reader::cancel_save_message(save_message_timer);
                    if(comic_open) close_comic();
                    else epub.close();
                    file.close(); open_name = nullptr;
                }
                if(book_open && !back_to_files) {
                    if(!globals_saved) {
                        show_overlay(save_ui, save_sprites, "Settings save failed");
                        reader::start_save_message(save_message_timer);
                    }
                    scene = Scene::READER; sprites.clear(); redraw_page = !comic_open; redraw_ui = false;
                } else {
                    scene = Scene::LIBRARY;
                    marquee.reset();
                    list_keys.reset();
                    if(!globals_saved) flash("Settings save failed");
                    redraw_ui = true;
                }
            }
        } else if(scene == Scene::VIEW) {
            ViewRow rows[VIEW_ROWS_MAX];
            const int count = view_rows(rows);
            if(list_keys.update(view_nav, count, false)) {
                // Step over the dividers (never first or last).
                if(view_row_label(rows[view_nav.selected])) view_nav.selected += rkey::up_held() ? -1 : 1;
                reader::list_show(view_nav, count, view_visible_rows());
                redraw_ui = true;
            }
            const ViewRow row = rows[view_nav.selected];
            if(row.kind == ViewRowKind::ZOOM && rkey::a_pressed()) {
                choose_zoom(row.value);
                redraw_ui = true;
            } else if((row.kind == ViewRowKind::COLUMNS || row.kind == ViewRowKind::ROWS) &&
                      (rkey::a_pressed() || rkey::left_pressed() || rkey::right_pressed())) {
                // Auto, then 1..9 screens, round again.
                uint8_t& counts = book_prefs.counts;
                const bool across = row.kind == ViewRowKind::COLUMNS;
                int n = across ? counts >> 4 : counts & 15;
                n = (n + (rkey::left_pressed() ? reader::comic_layout::MAX_COUNT : 1)) % (reader::comic_layout::MAX_COUNT + 1);
                counts = uint8_t(across ? (n << 4) | (counts & 15) : (counts & 0xF0) | n);
                redraw_ui = true;
            } else if(row.kind == ViewRowKind::ORDER && rkey::a_pressed()) {
                book_prefs.order = row.value;
                redraw_ui = true;
            } else if(row.kind == ViewRowKind::SKIP_EMPTY &&
                      (rkey::a_pressed() || rkey::left_pressed() || rkey::right_pressed())) {
                book_prefs.skip_empty = !book_prefs.skip_empty;
                redraw_ui = true;
            }
            if(rkey::b_pressed() || rkey::select_pressed()) {
                scene = Scene::SETTINGS; list_keys.reset(); redraw_ui = true;
            }
        } else if(scene == Scene::ANGLE) {
            const int u = list_keys.up.update(rkey::up_held(), rkey::up_pressed());
            const int d = list_keys.down.update(rkey::down_held(), rkey::down_pressed());
            if(u ? angle_step(angle_nav, -1, u == 1) : d && angle_step(angle_nav, 1, d == 1)) redraw_ui = true;
            const reader::Angle at = reader::Angle(angle_nav.selected);
            if(rkey::a_pressed() && angle_choosable(at) && at != settings.angle) {
                // The screen and the D-pad turn now; the cursor stays on the angle.
                settings.angle = at;
                apply_view();
                list_keys.reset();
                redraw_ui = true;
            }
            if(rkey::b_pressed() || rkey::select_pressed()) {
                // Settings may have more or fewer rows at the new angle.
                scene = Scene::SETTINGS;
                apply_view();
                reader::list_show(settings_nav, settings_count, screen::rows());
                list_keys.reset();
                redraw_ui = true;
            }
        } else if(scene == Scene::ANIMATIONS) {
            if(list_keys.update(animations_nav, ANIMATION_ROWS, false)) redraw_ui = true;
            const int delta = rkey::left_pressed() ? -1 : rkey::right_pressed() || rkey::a_pressed() ? 1 : 0;
            if(delta) {
                reader::Slide& slide = animation_slide(animations_nav.selected);
                slide = reader::Slide((int(slide) + reader::SLIDE_COUNT + delta) % reader::SLIDE_COUNT);
                redraw_ui = true;
            }
            if(rkey::b_pressed() || rkey::select_pressed()) {
                scene = Scene::SETTINGS; list_keys.reset(); redraw_ui = true;
            }
        } else if(scene == Scene::ABOUT) {
            if(rkey::left_pressed() && about_page > 0) { --about_page; about_scroll = 0; redraw_ui = true; }
            if(rkey::right_pressed() && about_page + 1 < reader::ABOUT_PAGE_COUNT) {
                ++about_page; about_scroll = 0; redraw_ui = true;
            }
            // A narrow (turned) screen wraps the text onto more rows: Up/Down scroll.
            if(rkey::up_pressed() && about_scroll > 0) { --about_scroll; redraw_ui = true; }
            if(rkey::down_pressed() && about_scroll + screen::rows() < about_count) {
                ++about_scroll; redraw_ui = true;
            }
            if(rkey::b_pressed() || rkey::select_pressed()) {
                scene = Scene::SETTINGS; list_keys.reset(); redraw_ui = true;
            }
        } else if(scene == Scene::IMPORT) {
            reader::ListNav& nav = import_nav[import_depth];
            const int count = import_ok ? reader::browse_count() : 0;
            if(list_keys.update(nav, count, true)) { marquee.reset(); redraw_ui = true; }
            if(rkey::b_pressed()) {
                BN_DATA_EWRAM_BSS static char parent[reader::BROWSE_PATH_MAX];
                std::memcpy(parent, reader::browse_folder(), sizeof(parent));
                if(reader::browse_parent(parent)) {
                    if(import_depth > 0) --import_depth;
                    else import_nav[0] = {};
                    enter_import(parent);
                } else {
                    scene = Scene::LIBRARY;
                    marquee.reset();
                    list_keys.reset();
                    redraw_ui = true;
                }
            } else if(rkey::a_pressed() && count) {
                const char* name = reader::browse_name(nav.selected);
                BN_DATA_EWRAM_BSS static char path[reader::BROWSE_PATH_MAX];
                if(!name || !reader::browse_join(path, reader::browse_folder(), name)) {
                    flash("Cannot open");
                } else if(reader::browse_is_folder(nav.selected)) {
                    if(import_depth + 1 < IMPORT_DEPTH) ++import_depth;
                    import_nav[import_depth] = {};
                    enter_import(path);
                } else {
                    std::memcpy(import_source, path, sizeof(import_source));
                    import_name = reader::browse_leaf(import_source);
                    confirm_nav = {};
                    list_keys.reset();
                    scene = Scene::IMPORT_CONFIRM;
                    redraw_ui = true;
                }
            }
        } else if(scene == Scene::IMPORT_CONFIRM) {
            if(list_keys.update(confirm_nav, 2, false)) redraw_ui = true;
            const bool yes = rkey::a_pressed() && confirm_nav.selected == 1;
            if(rkey::b_pressed() || (rkey::a_pressed() && !yes)) {
                scene = Scene::IMPORT; list_keys.reset(); redraw_ui = true;
            } else if(yes) {
                ImportProgress progress{painter};
                // No book is open here: the copy borrows the book memory.
                const auto result = reader::import_book(import_source, import_name, book_memory,
                                                        uint32_t(BOOK_MEMORY_BYTES), show_import_progress, &progress);
                reset_epub();
                if(result == reader::ImportResult::COPIED) {
                    reader::scan_library();
                    home = {};
                    for(int i = 0; i < reader::library_count(); ++i) {
                        const char* name = reader::library_name(i);
                        if(name && !std::strcmp(name, import_name)) { home.selected = i; break; }
                    }
                    reader::list_show(home, reader::library_count(), screen::ROWS);
                    scene = Scene::LIBRARY;
                    marquee.reset();
                    list_keys.reset();
                } else {
                    scene = Scene::IMPORT;
                    list_keys.reset();
                    // The folder listing may be stale after a failed write.
                    enter_import(reader::browse_folder());
                }
                flash(reader::import_result_string(result));
            }
        }

        // Reader mode overlay exit cleanup.
        if(scene != Scene::READER && reader_hold.mode_message_frames) {
            reader_hold.mode_message_frames = 0;
            sprites.clear();
        }
        // End reader mode overlay exit cleanup.
        if(scene == Scene::READER && reader::tick_save_message(save_message_timer))
            save_sprites.clear();
        if(redraw_page) { draw_page(painter); redraw_page = false; }
        book_open = open_name != nullptr;
        if(redraw_ui) {
            const bool turned_ui = apply_view() && scene != Scene::READER;
            uint8_t* pixels;
            if(turned_ui) {
                pixels = turn_canvas;
                screen::fill(pixels, 0, 0, screen::width(), screen::height(), screen::WHITE);
            } else {
                painter.fill(screen::WHITE);
                pixels = page_pixels(painter);
            }
            if(scene == Scene::LIBRARY) {
                const int count = storage_ok ? reader::library_count() : 0;
                if(!count) {
                    screen::header(pixels, "gbareader");
                    screen::center_text(pixels, 64, storage_ok ? "No books found" : "No SD card");
                    if(storage_ok) {
                        static constexpr char hint[] = "Start: Import books";
                        screen::small_draw(pixels, (240 - screen::small_width(hint)) / 2, 92, hint, screen::GREY);
                    }
                } else {
                    draw_list(pixels, "gbareader", home, count, library_label, no_folder, marquee.offset);
                }
            } else if(scene == Scene::IMPORT) {
                const int count = import_ok ? reader::browse_count() : 0;
                if(!count) {
                    screen::header(pixels, "Import to /gbareader");
                    screen::center_text(pixels, 64, import_ok ? "No books or folders here" : "Cannot read folder");
                } else {
                    draw_list(pixels, "Import to /gbareader", import_nav[import_depth], count,
                              import_label, reader::browse_is_folder, marquee.offset);
                }
            } else if(scene == Scene::IMPORT_CONFIRM) {
                screen::header(pixels, "Import into /gbareader?");
                screen::row(pixels, 0, "No", confirm_nav.selected == 0);
                screen::row(pixels, 1, "Yes", confirm_nav.selected == 1);
                screen::center_text(pixels, screen::ROW_Y + 3 * screen::ROW_H, import_name);
            } else if(scene == Scene::VIEW) {
                // Back on a menu: the reader's own colours are all it needs.
                draw_view_screen(pixels, view_nav, comic_page);
            } else if(scene == Scene::ANGLE) {
                screen::header(pixels, "Angle");
                for(int i = 0; i < reader::ANGLE_COUNT; ++i) {
                    const reader::Angle angle = reader::Angle(i);
                    const char* name = reader::angle_name(angle);
                    const bool choosable = angle_choosable(angle);
                    const int y = screen::ROW_Y + i * screen::ROW_H;
                    screen::row(pixels, i, choosable ? name : nullptr, i == angle_nav.selected, false);
                    if(!choosable)
                        screen::text_fit(pixels, screen::TEXT_X, y + 1, screen::width() - 2 * screen::TEXT_X,
                                         name, screen::GREY);
                    if(angle == settings.angle)
                        screen::tick(pixels, screen::TEXT_X + screen::text_width(name) + 5, y + 5, screen::BLACK);
                }
            } else if(scene == Scene::ANIMATIONS) {
                screen::header(pixels, "Animations");
                for(int i = 0; i < ANIMATION_ROWS; ++i) {
                    bn::string<40> label = animation_names[i];
                    label += ": ";
                    label += slide_name(animation_slide(i));
                    screen::row(pixels, i, label.data(), i == animations_nav.selected, false);
                }
            } else if(scene == Scene::ABOUT) {
                char where[8] = {char('1' + about_page), '/', char('0' + reader::ABOUT_PAGE_COUNT), 0};
                screen::header(pixels, reader::about_titles[about_page], where);
                BN_DATA_EWRAM_BSS static AboutRow rows[ABOUT_ROWS_MAX];
                about_count = about_layout(about_page, screen::width() - 2 * screen::TEXT_X, rows);
                if(about_scroll + screen::rows() > about_count)
                    about_scroll = about_count > screen::rows() ? about_count - screen::rows() : 0;
                for(int slot = 0; slot < screen::rows() && about_scroll + slot < about_count; ++slot) {
                    const AboutRow& row = rows[about_scroll + slot];
                    const int y = screen::ROW_Y + slot * screen::ROW_H;
                    char text[96];
                    const int n = row.length < int(sizeof(text)) - 1 ? row.length : int(sizeof(text)) - 1;
                    std::memcpy(text, row.text, size_t(n));
                    text[n] = 0;
                    if(row.heading) screen::small_draw(pixels, screen::TEXT_X, y + 6, text, screen::GREY);
                    else if(n) screen::text_fit(pixels, screen::TEXT_X, y + 1,
                                                screen::width() - 2 * screen::TEXT_X, text, screen::BLACK);
                }
            } else if(scene == Scene::SETTINGS) {
                screen::header(pixels, "Settings");
                for(int slot = 0; slot < screen::rows() && settings_nav.top + slot < settings_count; ++slot) {
                    const int index = settings_nav.top + slot;
                    const reader::SettingsItem item = settings_items[index];
                    if(item == reader::SettingsItem::BOOK_LABEL) {
                        screen::divider(pixels, slot, "This book");
                        continue;
                    }
                    bn::string<48> label, info;
                    if(item == reader::SettingsItem::GOTO && comic_open) {
                        info = "of "; info += bn::to_string<6>(comic.page_count());
                    } else if(item == reader::SettingsItem::GOTO) {
                        info = "Page ";
                        const int counted = reader::page_count_estimate(page_count);
                        if(counted >= 0) {
                            const int number = counted + 1 + page_turns;
                            if(page_count.state != reader::HistoryRebuildState::READY) info += "about ";
                            info += bn::to_string<12>(number > 0 ? number : 1);
                        } else {
                            info += "...";
                        }
                    } else if(item == reader::SettingsItem::LINE_SPACING) {
                        info = bn::to_string<4>(reader::lines_per_page(settings)); info += " lines";
                    } else if(item == reader::SettingsItem::VIEW && screen::width() > 200) {
                        info = reader::gbcomic::order_name(reader::gbcomic::Order(book_prefs.order));
                    }
                    screen::row(pixels, slot, settings_label(item, reader_hold.shoulder_page_turns,
                                                             goto_percent, confirm_leave, label),
                                index == settings_nav.selected, false, info.empty() ? nullptr : info.data());
                }
            }
            if(message && message_frames && scene != Scene::READER) screen::message(pixels, message);
            if(turned_ui) reader::turn_blit(turn_canvas, painter.page().data(), settings.angle);
            painter.flip_page_later();
            redraw_ui = false;
        } else if(!redraw_page && (scene == Scene::LIBRARY || scene == Scene::IMPORT)) {
            // Long name on the selected row: scroll it (gbamp3 marquee).
            const bool home_list = scene == Scene::LIBRARY;
            const int count = home_list ? (storage_ok ? reader::library_count() : 0) :
                                          (import_ok ? reader::browse_count() : 0);
            const reader::ListNav& nav = home_list ? home : import_nav[import_depth];
            if(count && !(message && message_frames)) {
                const char* text = home_list ? library_label(nav.selected) : import_label(nav.selected);
                const bool folder = !home_list && reader::browse_is_folder(nav.selected);
                if(marquee.step(screen::text_width(text), screen::row_text_width(folder, nullptr), frame)) {
                    copy_shown_page(painter);
                    screen::row(page_pixels(painter), nav.selected - nav.top, text, true, folder,
                                nullptr, unsigned(marquee.offset));
                    painter.flip_page_later();
                }
            }
        }
        bn::core::update();
    }
}
