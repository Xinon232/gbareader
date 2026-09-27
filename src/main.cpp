// gbareader V1.8 -- streaming Supercard SD TXT/EPUB reader.

#include "bn_bg_palette_item.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_items_ui_small_font.h"
#include "bn_sprite_items_ui_small_font_box.h"
#include "bn_sprite_items_ui_variable_8x16_font.h"
#include "bn_sprite_palette_item.h"
#include "bn_sprite_ptr.h"
#include "bn_string.h"
#include "bn_timer.h"
#include "bn_timers.h"
#include "bn_vector.h"

#include "common_variable_8x16_sprite_font.h"
#include "ui_small_font.h"
extern "C" {
#include "font_render.h"
}
#include "reader_core.h"
#include "reader_body.h"
#include "reader_open.h"
#include "reader_ui_state.h"
#include "reader_credits.h"
#include "reader_controls.h"
#include "reader_hold.h"
#include "epub_document.h"
#include "reader_file.h"

#include <cstring>

namespace {

using reader::Scene;

constexpr bn::color palette_colors[16] = {
    bn::color(31, 31, 31), bn::color(0, 0, 0), bn::color(12, 12, 12), bn::color(20, 20, 20),
    bn::color(), bn::color(), bn::color(), bn::color(), bn::color(), bn::color(), bn::color(),
    bn::color(), bn::color(), bn::color(), bn::color(), bn::color()
};
constexpr bn::bg_palette_item palette_item(bn::span<const bn::color>(palette_colors), bn::bpp_mode::BPP_8);

BN_DATA_EWRAM_BSS reader::ReaderFile file;
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

// gbamp3 grey (0x4210) for key hints; other UI text is black.
constexpr bn::color hint_colors[16] = {
    bn::color(31, 0, 31), bn::color(16, 16, 16), bn::color(31, 31, 31), bn::color(), bn::color(),
    bn::color(), bn::color(), bn::color(), bn::color(), bn::color(), bn::color(), bn::color(),
    bn::color(), bn::color(), bn::color(), bn::color()
};
constexpr bn::sprite_palette_item hint_palette_item(bn::span<const bn::color>(hint_colors), bn::bpp_mode::BPP_4);
// Background page layouts (Back history, page number) may use this much of each idle frame.
constexpr int BACKGROUND_WORK_TICKS = bn::timers::ticks_per_frame() / 2;

constexpr int UI_SPRITE_CAPACITY = 127;
constexpr int SAVE_OVERLAY_SPRITE_CAPACITY = 24;
constexpr int LIBRARY_VISIBLE_ROWS = reader::LIBRARY_VISIBLE_ROWS;
constexpr int LIBRARY_WORST_CASE_SPRITES =
        int(sizeof("gbareader V1.8") - 1) +
        int(sizeof("files: /gbareader") - 1) +
        int(sizeof("UP/DOWN select   A open") - 1) +
        int(sizeof("Select: Controls") - 1) + int(sizeof("Start: Credits") - 1) + 1;
static_assert(UI_SPRITE_CAPACITY <= 128);
static_assert(LIBRARY_WORST_CASE_SPRITES < 128);

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

void add_text(bn::sprite_text_generator& generator, int x, int y, const char* text,
              bn::vector<bn::sprite_ptr, UI_SPRITE_CAPACITY>& sprites)
{
    generator.generate(x, y, text, sprites);
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

void show_save_result(bn::sprite_text_generator& generator,
                      bn::vector<bn::sprite_ptr, SAVE_OVERLAY_SPRITE_CAPACITY>& sprites,
                      bool saved)
{
    show_overlay(generator, sprites, reader::save_result_string(saved));
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

}

int main()
{
    bn::core::init();
    bn::palette_bitmap_bg_ptr background = bn::palette_bitmap_bg_ptr::create(palette_item);
    bn::palette_bitmap_bg_painter painter(background);
    painter.fill(0);
    painter.flip_page_later();
    bn::core::update(); // Commit the initial flip before the first Home redraw.

    // UI text uses gbamp3's 5x7 font; the blue Butano font only draws the ">" cursor.
    bn::sprite_font ui_font(
            bn::sprite_items::ui_small_font, bn::utf8_characters_map_ref(),
            reader::ui_small_font_character_widths);
    bn::sprite_text_generator ui(ui_font);
    ui.set_palette_item(bn::sprite_items::ui_small_font.palette_item());
    bn::sprite_text_generator hint_ui(ui_font);
    hint_ui.set_palette_item(hint_palette_item);
    bn::sprite_font cursor_font(
            bn::sprite_items::ui_variable_8x16_font,
            common::variable_8x16_sprite_font_utf8_characters_map.reference(),
            common::variable_8x16_sprite_font_character_widths);
    bn::sprite_text_generator cursor_ui(cursor_font);
    cursor_ui.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());
    cursor_ui.set_left_alignment();
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
    Scene scene = Scene::LIBRARY;
    int selected = 0;
    int settings_row = 0;
    constexpr int GOTO_ROW = reader::SETTING_FIELD_COUNT;
    reader::Settings settings_before = settings;
    int goto_percent = 0;
    int goto_before = 0;
    int count_refresh_frames = 0;
    // Deliberately session-only: shoulder page turns always start disabled.
    reader::ReaderHold reader_hold{};
    bool redraw_ui = true;
    bool redraw_page = false;
    const char* open_name = nullptr;
    const reader::ByteSource* active_source = &file;
    const char* library_status = nullptr;
    reader::SaveMessageTimer save_message_timer{};
    bool pending_back = false;
    reader::CreditsInputGate credits_gate{};
    int controls_page = 0;

    while(true) {
        const Scene previous_scene = scene;
        // Reader hold sampling: keep edge history in every scene.
        if(reader_hold.mode_message_frames && !--reader_hold.mode_message_frames) sprites.clear();
        const unsigned reader_action = sample_reader_hold(reader_hold, scene);
        // End reader hold sampling.
        const bool any_held = bn::keypad::up_held() || bn::keypad::down_held() ||
                bn::keypad::left_held() || bn::keypad::right_held() ||
                bn::keypad::a_held() || bn::keypad::b_held() ||
                bn::keypad::start_held() || bn::keypad::select_held() ||
                bn::keypad::l_held() || bn::keypad::r_held();
        const int previous_controls_page = controls_page;
        const int previous_credits_page = credits_gate.page;
        const bool credits_consumed = reader::handle_controls_input(
                scene, credits_gate, controls_page, bn::keypad::select_pressed(),
                bn::keypad::b_pressed(), bn::keypad::left_pressed(),
                bn::keypad::right_pressed(), any_held) || reader::handle_credits_input(
                scene, credits_gate, bn::keypad::start_pressed(), bn::keypad::b_pressed(), any_held,
                bn::keypad::left_pressed(), bn::keypad::right_pressed());
        if(credits_consumed) {
            if(scene != previous_scene || controls_page != previous_controls_page ||
                    credits_gate.page != previous_credits_page) redraw_ui = true;
        } else if(scene == Scene::LIBRARY) {
            if(bn::keypad::up_pressed() && selected > 0) { --selected; library_status = nullptr; redraw_ui = true; }
            if(bn::keypad::down_pressed() && selected + 1 < reader::library_count()) { ++selected; library_status = nullptr; redraw_ui = true; }
            if(bn::keypad::a_pressed() && reader::library_count()) {
                library_status = nullptr;
                char path[reader::LIBRARY_PATH_MAX];
                if(!reader::library_path(selected, path) || !file.open_read_only(path)) {
                    library_status = "Book open failed";
                    redraw_ui = true;
                } else {
                  open_name = reader::library_name(selected);
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
                    if(opened == reader::OpenResult::SAVE_FAILED) {
                        // ON remains usable in RAM; later Start retries its persistence.
                        show_save_result(save_ui, save_sprites, false);
                        reader::start_save_message(save_message_timer);
                    }
                    scene = Scene::READER;
                    sprites.clear();
                    redraw_page = true;
                } else {
                    if(! library_status) library_status = epub_name(open_name) ?
                            reader::epub_error_string(epub.error()) : "Book read failed";
                    epub.close();
                    file.close();
                    open_name = nullptr;
                    redraw_ui = true;
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
                show_overlay(save_ui, sprites, reader_hold.shoulder_page_turns ? "L+R: On" : "L+R: Off", -64);
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
            } else if(reader_action == 2) {
                pending_back = false;
                settings_before = settings;
                goto_before = goto_percent = reader::page_percent(page, active_source->size());
                reader::cancel_save_message(save_message_timer);
                save_sprites.clear();
                scene = Scene::SETTINGS;
                redraw_ui = true;
            } else if(bn::keypad::start_pressed()) {
                pending_back = false;
                reader::TxtSaveFooter footer{page.start_offset, settings, history, history_rebuild};
                reader::cancel_save_message(save_message_timer);
                show_saving_overlay(save_ui, save_sprites);
                bn::core::update();
                const bool saved = file.save_footer(
                        footer, active_source == &epub ? active_source : nullptr);
                show_save_result(save_ui, save_sprites, saved);
                reader::start_save_message(save_message_timer);
            } else if(bn::keypad::select_pressed()) {
                pending_back = false;
                history_rebuild = {};
                reader::cancel_save_message(save_message_timer);
                save_sprites.clear();
                epub.close(); file.close(); open_name = nullptr;
                scene = Scene::LIBRARY; redraw_ui = true;
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
                library_status = reader::epub_error_string(epub.error());
                epub.close(); file.close(); open_name = nullptr;
                scene = Scene::LIBRARY; redraw_ui = true;
            }
        } else {
            // Keep counting pages while Settings is open, then show the number.
            if(scene == Scene::SETTINGS && reader::same_settings(settings_before, settings) &&
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
            if(bn::keypad::up_pressed() && settings_row > 0) { --settings_row; redraw_ui = true; }
            if(bn::keypad::down_pressed() && settings_row < GOTO_ROW) { ++settings_row; redraw_ui = true; }
            int delta = bn::keypad::left_pressed() ? -1 : bn::keypad::right_pressed() ? 1 : 0;
            if(settings_row == GOTO_ROW) {
                if(bn::keypad::l_pressed()) delta = -10;
                if(bn::keypad::r_pressed()) delta = 10;
                if(delta) {
                    goto_percent += delta;
                    if(goto_percent < 0) goto_percent = 0;
                    if(goto_percent > 100) goto_percent = 100;
                    redraw_ui = true;
                }
            } else if(delta) {
                reader::adjust_setting(settings, reader::SettingField(settings_row), delta);
                redraw_ui = true;
            }
            // A on Go to jumps now; B/Start apply everything and return as before.
            const bool go_now = settings_row == GOTO_ROW && bn::keypad::a_pressed();
            if(go_now || bn::keypad::b_pressed() || bn::keypad::start_pressed()) {
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
                pending_back = false;
                save_sprites.clear();
                scene = Scene::READER; sprites.clear(); redraw_page = true; redraw_ui = false;
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
        if(redraw_ui) {
            painter.fill(0); painter.flip_page_later();
            sprites.clear();
            ui.set_center_alignment();
            if(scene == Scene::LIBRARY) {
                add_text(ui, 0, -68, "gbareader V1.8", sprites);
                add_text(ui, 0, -48, "files: /gbareader", sprites);
                auto* pixels = reinterpret_cast<uint8_t*>(painter.page().data());
                if(!storage_ok || !reader::library_count()) {
                    const char* status = !storage_ok ? "SD or folder unavailable." : "No TXT/EPUB files found.";
                    draw_text_idx8_bus16_range(status, pixels + 56 * 240 + 8, 0, 224, 240, 1);
                    draw_text_idx8_bus16_range("Put TXT/EPUB in /gbareader", pixels + 78 * 240 + 8, 0, 224, 240, 1);
                    draw_text_idx8_bus16_range("on SD root, then restart.", pixels + 94 * 240 + 8, 0, 224, 240, 1);
                } else if(library_status) {
                    draw_text_idx8_bus16_range(library_status, pixels + 64 * 240 + 8, 0, 224, 240, 1);
                }
                const int first = reader::library_first_row(selected, reader::library_count());
                for(int i = first; ! library_status && i < reader::library_count() &&
                                   i < first + LIBRARY_VISIBLE_ROWS; ++i) {
                    const int y = 48 + (i - first) * 16;
                    if(i == selected) add_text(cursor_ui, 8 - 120, y - 72, ">", sprites);
                    draw_text_idx8_bus16_range(reader::library_name(i), pixels + y * 240 + 22, 0, 210, 240, 1);
                }
                hint_ui.set_center_alignment();
                add_text(hint_ui, 0, 56, "UP/DOWN select   A open", sprites);
                hint_ui.set_left_alignment();
                add_text(hint_ui, -104, 72, "Select: Controls", sprites);
                add_text(hint_ui, 8, 72, "Start: Credits", sprites);
            } else if(scene == Scene::CONTROLS) {
                add_text(ui, 0, -62, reader::controls_titles[controls_page], sprites);
                reader::draw_controls(reinterpret_cast<uint8_t*>(painter.page().data()), controls_page);
                hint_ui.set_center_alignment();
                add_text(hint_ui, 0, 68, "LEFT/RIGHT page   B back", sprites);
            } else if(scene == Scene::CREDITS) {
                add_text(ui, 0, -62, reader::credits_titles[credits_gate.page], sprites);
                reader::draw_credits(reinterpret_cast<uint8_t*>(painter.page().data()), credits_gate.page);
                hint_ui.set_center_alignment();
                add_text(hint_ui, 0, 68, "LEFT/RIGHT  B/START close", sprites);
            } else if(scene == Scene::SETTINGS) {
                add_text(ui, 0, -62, "Reader settings", sprites);
                // Rows share one left edge; the Butano ">" marks the selected row.
                constexpr int ROW_X = -60;
                const int row_y[GOTO_ROW + 1] = { -40, -24, 16 };
                add_text(cursor_ui, ROW_X - 12, row_y[settings_row], ">", sprites);
                ui.set_left_alignment();
                bn::string<48> spacing = "Line spacing: ";
                spacing += bn::to_string<4>(settings.line_spacing);
                add_text(ui, ROW_X, row_y[0], spacing.data(), sprites);
                bn::string<48> gap = "Paragraph gap: ";
                gap += reader::paragraph_gap_name(settings.paragraph_gap);
                add_text(ui, ROW_X, row_y[1], gap.data(), sprites);
                bn::string<48> lines = "Lines per page: ";
                lines += bn::to_string<4>(reader::lines_per_page(settings));
                add_text(ui, ROW_X, -8, lines.data(), sprites);
                bn::string<48> go = "Go to: "; go += bn::to_string<4>(goto_percent); go += "%";
                add_text(ui, ROW_X, row_y[GOTO_ROW], go.data(), sprites);
                bn::string<48> where = "Page ";
                const int counted = reader::page_count_estimate(page_count);
                if(counted >= 0) {
                    const int number = counted + 1 + page_turns;
                    if(page_count.state != reader::HistoryRebuildState::READY) where += "about ";
                    where += bn::to_string<12>(number > 0 ? number : 1);
                } else {
                    where += "...";
                }
                where += " - "; where += bn::to_string<4>(reader::page_percent(page, active_source->size()));
                where += "%";
                add_text(ui, ROW_X, 32, where.data(), sprites);
                hint_ui.set_center_alignment();
                add_text(hint_ui, 0, 64, settings_row == GOTO_ROW ? "A go  L/R 10%  B close" :
                                                                  "LEFT/RIGHT change  B close", sprites);
            }
            redraw_ui = false;
        }
        bn::core::update();
    }
}
