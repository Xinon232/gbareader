// GBA Reader v0.8.0 -- streaming Supercard SD TXT/EPUB reader.

#include "bn_bg_palette_item.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_palette_bitmap_bg_painter.h"
#include "bn_palette_bitmap_bg_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_items_ui_variable_8x16_font.h"
#include "bn_sprite_ptr.h"
#include "bn_string.h"
#include "bn_vector.h"

#include "common_variable_8x16_sprite_font.h"
extern "C" {
#include "font_render.h"
}
#include "reader_core.h"
#include "reader_ui_state.h"
#include "reader_credits.h"
#include "reader_controls.h"
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
reader::Settings settings;

constexpr int UI_SPRITE_CAPACITY = 127;
constexpr int SAVE_OVERLAY_SPRITE_CAPACITY = 16;
constexpr int LIBRARY_VISIBLE_ROWS = reader::LIBRARY_VISIBLE_ROWS;
constexpr int LIBRARY_WORST_CASE_SPRITES =
        int(sizeof("gbareader V1.0") - 1) +
        int(sizeof("files: /gbareader") - 1) +
        int(sizeof("UP/DOWN select   A open") - 1) +
        int(sizeof("Select: Controls") - 1) + int(sizeof("Start: Credits") - 1);
static_assert(UI_SPRITE_CAPACITY <= 128);
static_assert(LIBRARY_WORST_CASE_SPRITES < 128);

int glyph_width(uint32_t cp)
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

void draw_page(bn::palette_bitmap_bg_painter& painter)
{
    painter.fill(0);
    uint8_t* pixels = reinterpret_cast<uint8_t*>(painter.page().data());
    int y = settings.top_margin;
    for(int line = 0; line < page.line_count; ++line) {
        if(page.lines[line].text[0])
            draw_text_idx8_bus16_range(
                    page.lines[line].text,
                    pixels + y * 240 + reader::BODY_SIDE_MARGIN,
                    0,
                    reader::SCREEN_WIDTH - reader::BODY_SIDE_MARGIN * 2,
                    240,
                    1);
        if(line + 1 < page.line_count) {
            y += reader::FONT_HEIGHT + settings.line_spacing;
            if(page.lines[line].paragraph_break) y += reader::FONT_HEIGHT + settings.line_spacing;
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
                  bn::vector<bn::sprite_ptr, SAVE_OVERLAY_SPRITE_CAPACITY>& sprites,
                  const char* text)
{
    sprites.clear();
    generator.set_right_alignment();
    generator.set_bg_priority(0);
    generator.set_z_order(-32767);
    generator.generate(112, 64, text, sprites);
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

}

int main()
{
    bn::core::init();
    bn::palette_bitmap_bg_ptr background = bn::palette_bitmap_bg_ptr::create(palette_item);
    bn::palette_bitmap_bg_painter painter(background);
    painter.fill(0);
    painter.flip_page_later();
    bn::core::update(); // Commit the initial flip before the first Home redraw.

    bn::sprite_font ui_font(
            bn::sprite_items::ui_variable_8x16_font,
            common::variable_8x16_sprite_font_utf8_characters_map.reference(),
            common::variable_8x16_sprite_font_character_widths);
    bn::sprite_text_generator ui(ui_font);
    ui.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());
    bn::vector<bn::sprite_ptr, UI_SPRITE_CAPACITY> sprites;
    bn::sprite_text_generator save_ui(ui_font);
    save_ui.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());
    bn::vector<bn::sprite_ptr, SAVE_OVERLAY_SPRITE_CAPACITY> save_sprites;

    settings = reader::default_settings();
    bool storage_ok = reader::storage_init();
    Scene scene = Scene::LIBRARY;
    int selected = 0;
    int settings_row = 0;
    reader::Settings settings_before = settings;
    // Deliberately session-only: shoulder page turns always start disabled.
    bool shoulder_page_turns = false;
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
        const bool any_held = bn::keypad::up_held() || bn::keypad::down_held() ||
                bn::keypad::left_held() || bn::keypad::right_held() ||
                bn::keypad::a_held() || bn::keypad::b_held() ||
                bn::keypad::start_held() || bn::keypad::select_held() ||
                bn::keypad::l_held() || bn::keypad::r_held();
        const int previous_controls_page = controls_page;
        const bool credits_consumed = reader::handle_controls_input(
                scene, credits_gate, controls_page, bn::keypad::select_pressed(),
                bn::keypad::b_pressed(), bn::keypad::left_pressed(),
                bn::keypad::right_pressed(), any_held) || reader::handle_credits_input(
                scene, credits_gate, bn::keypad::start_pressed(), bn::keypad::b_pressed(), any_held);
        if(credits_consumed) {
            if(scene != previous_scene || controls_page != previous_controls_page) redraw_ui = true;
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
                uint32_t offset = 0;
                reader::TxtSaveFooter footer{};
                const bool footer_loaded = file.saved_footer(footer);
                if(footer_loaded) { settings = footer.settings; offset = footer.byte_offset; }
                bool page_open = ! library_status && reader::open_page_at(
                        *active_source, offset, settings, glyph_width, history, page);
                const bool saved_page_open = footer_loaded && page_open;
                if(! page_open && ! library_status)
                    page_open = reader::open_first_page(
                            *active_source, settings, glyph_width, history, page);
                if(page_open) {
                    history_rebuild = {};
                    if(saved_page_open) {
                        // Preserve current-layout fast Back; only legacy/unknown
                        // display layouts need a one-time lazy boundary rebuild.
                        reader::restore_saved_history(footer, history, history_rebuild);
                    }
                    pending_back = false;
                    reader::cancel_save_message(save_message_timer);
                    save_sprites.clear();
                    // Build the immutable ZIP cache after a usable first page exists.  This is
                    // intentionally not part of later bookmark saves, which append state only.
                    if(active_source == &epub && !epub.optimized_size()) {
                        show_overlay(save_ui, save_sprites, "Preparing cache...");
                        bn::core::update();
                        reader::TxtSaveFooter cache_state{page.start_offset, settings, history,
                                                          history_rebuild};
                        if(file.save_footer(cache_state, &epub)) {
                            epub.close();
                            if(epub.open(file)) active_source = &epub;
                        }
                        save_sprites.clear();
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
            reader::Page next{};
            const bool forward_pressed = bn::keypad::right_pressed() || bn::keypad::a_pressed() ||
                                         (shoulder_page_turns && bn::keypad::r_pressed());
            const bool back_pressed = bn::keypad::left_pressed() || bn::keypad::b_pressed() ||
                                      (shoulder_page_turns && bn::keypad::l_pressed());
            if(bn::keypad::up_pressed()) {
                shoulder_page_turns = ! shoulder_page_turns;
            } else if(forward_pressed) {
                if(pending_back) save_sprites.clear();
                pending_back = false;
                if(reader::next_page(*active_source, settings, glyph_width, history, page, next)) {
                    page = next;
                    redraw_page = true;
                }
            } else if(back_pressed) {
                if(reader::previous_page(*active_source, settings, glyph_width, history, next)) {
                    page = next;
                    redraw_page = true;
                } else if(history_rebuild.state == reader::HistoryRebuildState::BUILDING ||
                          history_rebuild.state == reader::HistoryRebuildState::READY) {
                    pending_back = true;
                    reader::cancel_save_message(save_message_timer);
                    show_overlay(save_ui, save_sprites, "Loading back...");
                }
            } else if(bn::keypad::down_pressed()) {
                pending_back = false;
                settings_before = settings;
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
            if(scene == Scene::READER && idle_frame &&
               history_rebuild.state == reader::HistoryRebuildState::BUILDING)
                reader::step_history_rebuild(
                        *active_source, settings, glyph_width, history_rebuild);
            if(scene == Scene::READER && history.count == 0 &&
               history_rebuild.state == reader::HistoryRebuildState::READY &&
               page.start_offset == history_rebuild.anchor) {
                reader::adopt_rebuilt_history(history_rebuild, history);
                if(pending_back) {
                    pending_back = false;
                    save_sprites.clear();
                    if(reader::previous_page(
                            *active_source, settings, glyph_width, history, next)) {
                        page = next;
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
            if(bn::keypad::up_pressed() && settings_row > 0) { --settings_row; redraw_ui = true; }
            if(bn::keypad::down_pressed() && settings_row < 2) { ++settings_row; redraw_ui = true; }
            int delta = bn::keypad::left_pressed() ? -1 : bn::keypad::right_pressed() ? 1 : 0;
            if(delta) {
                reader::adjust_setting(settings, reader::SettingField(settings_row), delta);
                redraw_ui = true;
            }
            if(bn::keypad::b_pressed() || bn::keypad::start_pressed()) {
                if(!reader::same_settings(settings_before, settings)) {
                    const uint32_t resume_offset = page.start_offset;
                    reader::open_page_at(*active_source, resume_offset, settings, glyph_width, history, page);
                    reader::begin_history_rebuild(resume_offset, history_rebuild);
                }
                pending_back = false;
                save_sprites.clear();
                scene = Scene::READER; sprites.clear(); redraw_page = true; redraw_ui = false;
            }
        }

        if(scene == Scene::READER && reader::tick_save_message(save_message_timer))
            save_sprites.clear();
        if(redraw_page) { draw_page(painter); redraw_page = false; }
        if(redraw_ui) {
            painter.fill(0); painter.flip_page_later();
            sprites.clear();
            ui.set_center_alignment();
            if(scene == Scene::LIBRARY) {
                add_text(ui, 0, -68, "gbareader V1.0", sprites);
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
                    const int y = 52 + (i - first) * 16;
                    draw_text_idx8_bus16_range(i == selected ? ">" : " ", pixels + y * 240 + 8, 0, 12, 240, 1);
                    draw_text_idx8_bus16_range(reader::library_name(i), pixels + y * 240 + 22, 0, 210, 240, 1);
                }
                add_text(ui, 0, 44, "UP/DOWN select   A open", sprites);
                ui.set_left_alignment();
                add_text(ui, -104, 68, "Select: Controls", sprites);
                add_text(ui, 8, 68, "Start: Credits", sprites);
            } else if(scene == Scene::CONTROLS) {
                add_text(ui, 0, -62, reader::controls_titles[controls_page], sprites);
                reader::draw_controls(reinterpret_cast<uint8_t*>(painter.page().data()), controls_page);
                add_text(ui, 0, 68, "LEFT/RIGHT page   B back", sprites);
            } else if(scene == Scene::CREDITS) {
                add_text(ui, 0, -62, "Credits", sprites);
                reader::draw_credits(reinterpret_cast<uint8_t*>(painter.page().data()));
                add_text(ui, 0, 68, "B/START close", sprites);
            } else if(scene == Scene::SETTINGS) {
                add_text(ui, 0, -62, "Reader settings", sprites);
                const char* labels[3] = { "Line spacing", "Top margin", "Bottom margin" };
                int values[3] = { settings.line_spacing, settings.top_margin,
                                  settings.bottom_margin };
                for(int i = 0; i < 3; ++i) {
                    bn::string<48> row = i == settings_row ? "> " : "  ";
                    row += labels[i]; row += ": "; row += bn::to_string<4>(values[i]);
                    add_text(ui, 0, -28 + i * 22, row.data(), sprites);
                }
                add_text(ui, 0, 54, "LEFT/RIGHT change", sprites);
                add_text(ui, 0, 68, "B/START close", sprites);
            }
            redraw_ui = false;
        }
        bn::core::update();
    }
}
