// gbareader V3.1 -- streaming Supercard SD TXT/EPUB reader.

#include "bn_bg_palette_item.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_memory.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_items_ui_small_font_box.h"
#include "bn_sprite_palette_item.h"
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

#include <cstring>

namespace {

using reader::Scene;
namespace screen = reader::screen;

// 0-3: book page (white, black, Arabic greys); 4-5: gbamp3 light blue and grey;
// 6-7: green / red status marks (demo start screen).
constexpr bn::color palette_colors[16] = {
    bn::color(31, 31, 31), bn::color(0, 0, 0), bn::color(12, 12, 12), bn::color(20, 20, 20),
    bn::color(21, 26, 31), bn::color(16, 16, 16), bn::color(4, 20, 6), bn::color(27, 4, 4), bn::color(), bn::color(),
    bn::color(), bn::color(), bn::color(), bn::color(), bn::color(), bn::color()
};
constexpr bn::bg_palette_item palette_item(bn::span<const bn::color>(palette_colors), bn::bpp_mode::BPP_8);

BN_DATA_EWRAM_BSS reader::ReaderFile file;
BN_DATA_EWRAM_BSS reader::GlobalSettingsStore global_settings;
static_assert(reader::GLOBAL_BOOK_NAME_MAX == reader::LIBRARY_NAME_MAX);
BN_DATA_EWRAM_BSS reader::EpubDocument epub;
BN_DATA_EWRAM_BSS reader::Page page;
BN_DATA_EWRAM_BSS reader::PageHistory history;
BN_DATA_EWRAM_BSS reader::PageHistoryRebuild history_rebuild;
BN_DATA_EWRAM_BSS reader::PageCount page_count;
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
    painter.fill(0);
    uint8_t* pixels = reinterpret_cast<uint8_t*>(painter.page().data());
    int y = reader::page_top(settings);
    for(int line = 0; line < page.line_count; ++line) {
        if(page.lines[line].text[0])
            reader::draw_body_line(
                    page.lines[line].text,
                    pixels + y * 240 + reader::BODY_SIDE_MARGIN, settings.arabic_shaping);
        if(line + 1 < page.line_count) {
            y += reader::FONT_HEIGHT + settings.line_spacing;
            if(page.lines[line].paragraph_break) y += reader::paragraph_gap_pixels(settings);
        }
    }
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

void show_overlay(bn::sprite_text_generator& generator,
                  bn::ivector<bn::sprite_ptr>& sprites,
                  const char* text, int y = 64)
{
    sprites.clear();
    generator.set_right_alignment();
    generator.set_bg_priority(0);
    generator.set_z_order(-32767);
    // White-box glyphs, one sprite each, join into one label over book text; the box
    // font's blank '~' keeps spaces inside the box.
    bn::string<32> boxed(text);
    for(char& c : boxed) if(c == ' ') c = '~';
    generator.set_one_sprite_per_character(true);
    generator.generate(112, y, boxed, sprites);
    generator.set_one_sprite_per_character(false);
    for(bn::sprite_ptr& sprite : sprites) sprite.put_above();
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
    const unsigned keys = unsigned(bn::keypad::up_held()) |
            (unsigned(bn::keypad::down_held()) << 1) |
            (unsigned(bn::keypad::left_held()) << 2) |
            (unsigned(bn::keypad::right_held()) << 3) |
            (unsigned(bn::keypad::a_held()) << 4) |
            (unsigned(bn::keypad::b_held()) << 5) |
            (unsigned(bn::keypad::start_held()) << 6) |
            (unsigned(bn::keypad::select_held()) << 7) |
            (unsigned(bn::keypad::l_held()) << 8) |
            (unsigned(bn::keypad::r_held()) << 9);
    return hold.update(keys, scene == Scene::READER);
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
        const int u = up.update(bn::keypad::up_held(), bn::keypad::up_pressed());
        const int d = down.update(bn::keypad::down_held(), bn::keypad::down_pressed());
        if(u) moved |= reader::list_step(nav, count, screen::ROWS, -1, u == 1);
        else if(d) moved |= reader::list_step(nav, count, screen::ROWS, 1, d == 1);
        if(paging) {
            const int l = left.update(bn::keypad::left_held(), bn::keypad::left_pressed());
            const int r = right.update(bn::keypad::right_held(), bn::keypad::right_pressed());
            if(l) moved |= reader::list_page(nav, count, screen::ROWS, -1);
            else if(r) moved |= reader::list_page(nav, count, screen::ROWS, 1);
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
    return name ? name : "?";
}

const char* import_label(int index)
{
    const char* name = reader::browse_name(index);
    return name ? name : "?";
}

void draw_list(uint8_t* pixels, const char* title, const reader::ListNav& nav, int count,
               const char* (*label)(int), bool (*folder)(int), unsigned skip)
{
    screen::header(pixels, title);
    for(int slot = 0; slot < screen::ROWS && nav.top + slot < count; ++slot) {
        const int i = nav.top + slot;
        screen::row(pixels, slot, label(i), i == nav.selected, folder && folder(i), nullptr,
                    i == nav.selected ? skip : 0);
    }
}

bool no_folder(int) { return false; }

// Demo build notices: what is not possible, where the full version is.
constexpr const char* demo_import_notice[] = {
    "Importing files is not possible", "in demo version.", "", "Get the full version at", "halimj.itch.io", "",
    "Press any button to continue."
};
constexpr const char* demo_save_notice[] = {
    "Saving page is not possible", "in demo version.", "", "Get the full version at", "halimj.itch.io", "",
    "Press any button to continue."
};

const char* settings_label(reader::SettingsItem item, bool shoulder, int goto_percent,
                           bool confirm_leave, bn::string<48>& out)
{
    using reader::SettingsItem;
    out.clear();
    switch(item) {
    case SettingsItem::GOTO:
        out = "Go to: "; out += bn::to_string<4>(goto_percent); out += "%"; break;
    case SettingsItem::LINE_SPACING:
        out = "Line spacing: "; out += bn::to_string<4>(settings.line_spacing); break;
    case SettingsItem::PARAGRAPH_GAP:
        out = "Paragraph gap: "; out += reader::paragraph_gap_name(settings.paragraph_gap); break;
    case SettingsItem::PAGE_TURN_KEYS:
        out = "L/R page turns: "; out += shoulder ? "On" : "Off"; break;
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

int main()
{
    bn::core::init();
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
    Scene scene = GBAREADER_DEMO ? Scene::DEMO_INTRO : Scene::LIBRARY;
    // Demo notice: its text and the screen a button press returns to.
    const char* const* demo_notice = demo_import_notice;
    Scene demo_return = Scene::LIBRARY;
    reader::ListNav home{};
    home.selected = storage_ok ? reader::remembered_library_selection(
            global_settings.values.last_book, reader::library_count(), reader::library_name) : 0;
    reader::list_show(home, reader::library_count(), screen::ROWS);
    reader::ListNav settings_nav{};
    reader::SettingsItem settings_items[reader::SETTINGS_MAX_ROWS];
    int settings_count = 0;
    reader::Settings settings_before = settings;
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
    bool pending_back = false;
    ListKeys list_keys;
    reader::KeyRepeat goto_left, goto_right;
    reader::Marquee marquee;
    unsigned frame = 0;
    // Import browser: one remembered cursor per folder level.
    reader::ListNav import_nav[IMPORT_DEPTH]{};
    int import_depth = 0;
    bool import_ok = false;
    reader::ListNav confirm_nav{};
    char import_source[reader::BROWSE_PATH_MAX]{};
    const char* import_name = nullptr;

    auto flash = [&](const char* text) {
        message = text;
        message_frames = MESSAGE_FRAMES;
        redraw_ui = true;
    };
    auto show_demo_notice = [&](const char* const* notice, Scene back) {
        demo_notice = notice;
        demo_return = back;
        sprites.clear();
        save_sprites.clear();
        scene = Scene::DEMO_NOTICE;
        redraw_ui = true;
    };
    (void)show_demo_notice;
    auto open_settings = [&](bool book_open) {
        settings_count = reader::settings_items(book_open, settings_items);
        settings_nav = {};
        confirm_leave = false;
        if(!book_open) {
            settings.line_spacing = global_settings.values.line_spacing;
            settings.paragraph_gap = global_settings.values.paragraph_gap;
        }
        settings_before = settings;
        list_keys.reset();
        scene = Scene::SETTINGS;
        redraw_ui = true;
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
        ++frame;
        bool book_open = open_name != nullptr;
        // Reader hold sampling: keep edge history in every scene.
        if(reader_hold.mode_message_frames && !--reader_hold.mode_message_frames) sprites.clear();
        const unsigned reader_action = sample_reader_hold(reader_hold, scene);
        // End reader hold sampling.
        if(scene != Scene::READER && message_frames && !--message_frames) {
            message = nullptr;
            redraw_ui = true;
        }
        if(scene == Scene::LIBRARY) {
            const int count = storage_ok ? reader::library_count() : 0;
            if(list_keys.update(home, count, true)) { marquee.reset(); redraw_ui = true; }
            if(bn::keypad::select_pressed()) {
                open_settings(false);
            } else if(bn::keypad::start_pressed()) {
                if(GBAREADER_DEMO) show_demo_notice(demo_import_notice, Scene::LIBRARY);
                else if(!storage_ok) flash("No SD card");
                else { import_depth = 0; import_nav[0] = {}; enter_import("/"); }
            } else if(bn::keypad::a_pressed() && count) {
                const char* library_status = nullptr;
                char path[reader::LIBRARY_PATH_MAX];
                if(!reader::library_path(home.selected, path) || !file.open_read_only(path)) {
                    flash("Book open failed");
                } else {
                  open_name = reader::library_name(home.selected);
                active_source = &file;
                if(epub_name(open_name)) {
                    if(epub.open(file)) active_source = &epub;
                    else library_status = reader::epub_error_string(epub.error());
                }
                reader::TxtSaveFooter footer{};
                const bool footer_loaded = file.saved_footer(footer);
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
                        *active_source, footer_loaded ? &footer : nullptr, settings, glyph_width,
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
                    flash(library_status);
                }
                }
            }
        } else if(scene == Scene::READER) {
            // Few pages left for Back: load the window before the oldest one ahead of time.
            if(history_rebuild.state == reader::HistoryRebuildState::IDLE &&
               history.count < reader::HISTORY_PREFETCH_PAGES) {
                const uint32_t older_end = reader::history_rebuild_anchor(history, page.start_offset);
                if(older_end > 0) reader::begin_history_rebuild(older_end, history_rebuild);
            }
            reader::Page next{};
            const bool forward_pressed = bn::keypad::right_pressed() || bn::keypad::a_pressed() ||
                                         (reader_hold.shoulder_page_turns && bn::keypad::r_pressed());
            const bool back_pressed = bn::keypad::left_pressed() || bn::keypad::b_pressed() ||
                                      (reader_hold.shoulder_page_turns && bn::keypad::l_pressed());
            if(reader_action == 1) {
                reader_hold.shoulder_page_turns = ! reader_hold.shoulder_page_turns;
                global_settings.values.shoulder_page_turns = reader_hold.shoulder_page_turns;
                const bool mode_saved = global_settings.save();
                show_overlay(save_ui, sprites, !mode_saved ? "Settings save failed" :
                             reader_hold.shoulder_page_turns ? "L+R: On" : "L+R: Off", -64);
                reader_hold.mode_message_frames = 60; // One second of application frames, non-blocking.
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
                    pending_back = true;
                    reader::cancel_save_message(save_message_timer);
                    show_overlay(save_ui, save_sprites, "Loading back...");
                }
            } else if(bn::keypad::start_pressed()) {
                pending_back = false;
#if GBAREADER_DEMO
                reader::cancel_save_message(save_message_timer);
                show_demo_notice(demo_save_notice, Scene::READER);
            } else if(false) {
#endif
                reader::TxtSaveFooter footer{page.start_offset, settings, history, history_rebuild};
                reader::cancel_save_message(save_message_timer);
                show_saving_overlay(save_ui, save_sprites);
                bn::core::update();
                const bool saved = file.save_footer(
                        footer, active_source == &epub ? active_source : nullptr);
                if(saved) saved_offset = page.start_offset;
                const bool globals_saved = global_settings.save();
                show_overlay(save_ui, save_sprites, reader::save_status_message(saved, globals_saved));
                reader::start_save_message(save_message_timer);
            } else if(bn::keypad::select_pressed()) {
                pending_back = false;
                goto_before = goto_percent = reader::page_percent(page, active_source->size());
                reader::cancel_save_message(save_message_timer);
                save_sprites.clear();
                open_settings(true);
            }

            const bool idle_frame = !bn::keypad::up_pressed() && !bn::keypad::down_pressed() &&
                                    !bn::keypad::left_pressed() && !bn::keypad::right_pressed() &&
                                    !bn::keypad::a_pressed() && !bn::keypad::b_pressed() &&
                                    !bn::keypad::start_pressed() && !bn::keypad::select_pressed() &&
                                    !bn::keypad::l_pressed() && !bn::keypad::r_pressed();
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
                if(!merged) history_rebuild = {};
                else if(pending_back) {
                    pending_back = false;
                    save_sprites.clear();
                    if(reader::previous_page(
                            *active_source, settings, glyph_width, history, next)) {
                        page = next;
                        --page_turns;
                        redraw_page = true;
                    }
                }
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
                scene = Scene::LIBRARY;
                flash(error);
            }
        } else if(scene == Scene::SETTINGS) {
            // Keep counting pages while Settings is open, then show the number.
            if(book_open && reader::same_settings(settings_before, settings) &&
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
            if(list_keys.update(settings_nav, settings_count, false)) { confirm_leave = false; redraw_ui = true; }
            const reader::SettingsItem item = settings_items[settings_nav.selected];
            int delta = bn::keypad::left_pressed() ? -1 : bn::keypad::right_pressed() ? 1 : 0;
            bool close = bn::keypad::b_pressed() || bn::keypad::select_pressed() ||
                         (!GBAREADER_DEMO && bn::keypad::start_pressed());
            if(GBAREADER_DEMO && bn::keypad::start_pressed()) {
                // Demo: Start is the save button; say why it does nothing.
                show_demo_notice(demo_save_notice, Scene::SETTINGS);
                close = false;
            }
            if(confirm_leave && bn::keypad::b_pressed()) {
                // B takes the question back; Settings stays open.
                confirm_leave = false;
                close = false;
                redraw_ui = true;
            }
            bool go_now = false, back_to_files = false;
            if(item == reader::SettingsItem::GOTO) {
                const int l = goto_left.update(bn::keypad::left_held(), bn::keypad::left_pressed());
                const int r = goto_right.update(bn::keypad::right_held(), bn::keypad::right_pressed());
                delta = l ? -1 : r ? 1 : 0;
                if(bn::keypad::l_pressed()) delta = -10;
                if(bn::keypad::r_pressed()) delta = 10;
                if(delta) {
                    goto_percent += delta;
                    if(goto_percent < 0) goto_percent = 0;
                    if(goto_percent > 100) goto_percent = 100;
                    redraw_ui = true;
                }
                // A on Go to jumps now; B/Select/Start apply everything and return as before.
                go_now = bn::keypad::a_pressed();
            } else if(item == reader::SettingsItem::PAGE_TURN_KEYS) {
                if(bn::keypad::a_pressed()) delta = reader_hold.shoulder_page_turns ? -1 : 1;
                if(delta) {
                    reader_hold.shoulder_page_turns = delta > 0;
                    redraw_ui = true;
                }
            } else if(item == reader::SettingsItem::LINE_SPACING ||
                      item == reader::SettingsItem::PARAGRAPH_GAP) {
                const auto field = item == reader::SettingsItem::LINE_SPACING ?
                        reader::SettingField::LINE_SPACING : reader::SettingField::PARAGRAPH_GAP;
                if(bn::keypad::a_pressed()) {
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
                if(bn::keypad::a_pressed()) {
                    // The demo cannot save, so there is nothing to ask.
                    if(confirm_leave || page.start_offset == saved_offset || GBAREADER_DEMO) back_to_files = true;
                    else { confirm_leave = true; redraw_ui = true; }
                }
            } else if(bn::keypad::a_pressed()) {
                about_page = 0;
                scene = Scene::ABOUT;
                redraw_ui = true;
            }
            if(go_now || close || back_to_files) {
                if(book_open && !back_to_files) {
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
                const bool globals_saved = global_settings.save();
                if(back_to_files) {
                    history_rebuild = {};
                    reader::cancel_save_message(save_message_timer);
                    epub.close(); file.close(); open_name = nullptr;
                }
                if(book_open && !back_to_files) {
                    if(!globals_saved) {
                        show_overlay(save_ui, save_sprites, "Settings save failed");
                        reader::start_save_message(save_message_timer);
                    }
                    scene = Scene::READER; sprites.clear(); redraw_page = true; redraw_ui = false;
                } else {
                    scene = Scene::LIBRARY;
                    marquee.reset();
                    list_keys.reset();
                    if(!globals_saved) flash("Settings save failed");
                    redraw_ui = true;
                }
            }
        } else if(scene == Scene::ABOUT) {
            if(bn::keypad::left_pressed() && about_page > 0) { --about_page; redraw_ui = true; }
            if(bn::keypad::right_pressed() && about_page + 1 < reader::ABOUT_PAGE_COUNT) {
                ++about_page; redraw_ui = true;
            }
            if(bn::keypad::b_pressed() || bn::keypad::select_pressed()) {
                scene = Scene::SETTINGS; list_keys.reset(); redraw_ui = true;
            }
        } else if(scene == Scene::IMPORT) {
            reader::ListNav& nav = import_nav[import_depth];
            const int count = import_ok ? reader::browse_count() : 0;
            if(list_keys.update(nav, count, true)) { marquee.reset(); redraw_ui = true; }
            if(bn::keypad::b_pressed()) {
                char parent[reader::BROWSE_PATH_MAX];
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
            } else if(bn::keypad::a_pressed() && count) {
                const char* name = reader::browse_name(nav.selected);
                char path[reader::BROWSE_PATH_MAX];
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
        } else if(scene == Scene::DEMO_INTRO) {
            if(bn::keypad::a_pressed()) { scene = Scene::LIBRARY; list_keys.reset(); redraw_ui = true; }
        } else if(scene == Scene::DEMO_NOTICE) {
            if(bn::keypad::any_pressed()) {
                scene = demo_return;
                list_keys.reset();
                if(scene == Scene::READER) redraw_page = true;
                else redraw_ui = true;
            }
        } else if(scene == Scene::IMPORT_CONFIRM) {
            if(list_keys.update(confirm_nav, 2, false)) redraw_ui = true;
            const bool yes = bn::keypad::a_pressed() && confirm_nav.selected == 1;
            if(bn::keypad::b_pressed() || (bn::keypad::a_pressed() && !yes)) {
                scene = Scene::IMPORT; list_keys.reset(); redraw_ui = true;
            } else if(yes) {
                ImportProgress progress{painter};
                const auto result = reader::import_book(import_source, import_name,
                                                        show_import_progress, &progress);
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
            painter.fill(screen::WHITE);
            uint8_t* pixels = page_pixels(painter);
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
            } else if(scene == Scene::DEMO_INTRO) {
                const auto found = reader::storage_status();
                const int books = storage_ok ? reader::library_count() : 0;
                bn::string<48> count = bn::to_string<12>(books);
                count += books == 1 ? " book found in /gbareader" : " books found in /gbareader";
                screen::header(pixels, "gbareader demo");
                screen::center_text(pixels, 30, count.data());
                screen::text_fit(pixels, 44, 62, 150, "Flashcart compatible", screen::BLACK);
                screen::status_mark(pixels, 196, 62, found.flashcart);
                screen::text_fit(pixels, 44, 88, 150, "SD card", screen::BLACK);
                screen::status_mark(pixels, 196, 88, found.sd_card);
                screen::center_text(pixels, 128, "Press A to continue.");
            } else if(scene == Scene::DEMO_NOTICE) {
                screen::header(pixels, "gbareader demo");
                for(int i = 0; i < 7; ++i)
                    if(demo_notice[i][0]) screen::center_text(pixels, 22 + i * 18, demo_notice[i]);
            } else if(scene == Scene::IMPORT_CONFIRM) {
                screen::header(pixels, "Import into /gbareader?");
                screen::row(pixels, 0, "No", confirm_nav.selected == 0);
                screen::row(pixels, 1, "Yes", confirm_nav.selected == 1);
                screen::center_text(pixels, screen::ROW_Y + 3 * screen::ROW_H, import_name);
            } else if(scene == Scene::ABOUT) {
                char where[8] = {char('1' + about_page), '/', char('0' + reader::ABOUT_PAGE_COUNT), 0};
                screen::header(pixels, reader::about_titles[about_page], where);
                for(int i = 0; i < reader::ABOUT_LINES; ++i) {
                    const char* line = reader::about_lines[about_page][i];
                    const int y = screen::ROW_Y + i * screen::ROW_H;
                    if(line[0] == '#') screen::small_draw(pixels, screen::TEXT_X, y + 6, line + 1, screen::GREY);
                    else if(line[0]) screen::text_fit(pixels, screen::TEXT_X, y + 1, 228, line, screen::BLACK);
                }
            } else if(scene == Scene::SETTINGS) {
                screen::header(pixels, "Settings");
                for(int slot = 0; slot < settings_count; ++slot) {
                    const reader::SettingsItem item = settings_items[slot];
                    bn::string<48> label, info;
                    if(item == reader::SettingsItem::GOTO) {
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
                    }
                    screen::row(pixels, slot, settings_label(item, reader_hold.shoulder_page_turns,
                                                             goto_percent, confirm_leave, label),
                                slot == settings_nav.selected, false, info.empty() ? nullptr : info.data());
                }
            }
            if(message && message_frames && scene != Scene::READER) screen::message(pixels, message);
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
