// GBA Reader v0.4.0 -- read-only Supercard SD TXT/EPUB reader.

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
#include "epub_document.h"
#include "reader_file.h"

#include <cstring>

namespace {

enum class Scene { LIBRARY, READER, SETTINGS };

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
reader::Settings settings;

constexpr int UI_SPRITE_CAPACITY = 127;
constexpr int LIBRARY_VISIBLE_ROWS = 4;
constexpr int LIBRARY_DISPLAY_CHARACTERS = 15;
constexpr int LIBRARY_WORST_CASE_SPRITES =
        int(sizeof("GBA Reader v0.4.0") - 1) +
        LIBRARY_VISIBLE_ROWS * (2 + LIBRARY_DISPLAY_CHARACTERS) +
        int(sizeof("UP/DOWN select   A open") - 1);
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
        y += reader::FONT_HEIGHT + settings.line_spacing;
        if(page.lines[line].paragraph_break) y += reader::FONT_HEIGHT + settings.line_spacing;
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

void library_display_name(const char* name, char* output)
{
    int input = 0;
    int out = 0;
    int characters = 0;
    while(name[input] && characters < LIBRARY_DISPLAY_CHARACTERS) {
        unsigned char lead = static_cast<unsigned char>(name[input]);
        int bytes = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 :
                    (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
        int available = 1;
        while(available < bytes && name[input + available] &&
              (static_cast<unsigned char>(name[input + available]) & 0xC0) == 0x80) ++available;
        if(available != bytes) bytes = 1;
        for(int i = 0; i < bytes; ++i) output[out++] = name[input++];
        ++characters;
    }
    output[out] = 0;
}

}

int main()
{
    bn::core::init();
    bn::palette_bitmap_bg_ptr background = bn::palette_bitmap_bg_ptr::create(palette_item);
    bn::palette_bitmap_bg_painter painter(background);
    painter.fill(0);
    painter.flip_page_later();

    bn::sprite_font ui_font(
            bn::sprite_items::ui_variable_8x16_font,
            common::variable_8x16_sprite_font_utf8_characters_map.reference(),
            common::variable_8x16_sprite_font_character_widths);
    bn::sprite_text_generator ui(ui_font);
    ui.set_palette_item(bn::sprite_items::ui_variable_8x16_font.palette_item());
    bn::vector<bn::sprite_ptr, UI_SPRITE_CAPACITY> sprites;

    settings = reader::default_settings();
    bool storage_ok = reader::storage_init();
    Scene scene = Scene::LIBRARY;
    int selected = 0;
    int settings_row = 0;
    bool redraw_ui = true;
    bool redraw_page = false;
    const char* open_name = nullptr;
    const reader::ByteSource* active_source = &file;
    const char* library_status = nullptr;

    while(true) {
        if(scene == Scene::LIBRARY) {
            if(bn::keypad::up_pressed() && selected > 0) { --selected; library_status = nullptr; redraw_ui = true; }
            if(bn::keypad::down_pressed() && selected + 1 < reader::library_count()) { ++selected; library_status = nullptr; redraw_ui = true; }
            if(bn::keypad::a_pressed() && reader::library_count()) {
                library_status = nullptr;
                if(! file.open_read_only(reader::library_name(selected))) {
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
                if(file.saved_footer(footer)) { settings = footer.settings; offset = footer.byte_offset; }
                bool page_open = ! library_status && reader::open_page_at(
                        *active_source, offset, settings, glyph_width, history, page);
                if(! page_open && ! library_status)
                    page_open = reader::open_first_page(
                            *active_source, settings, glyph_width, history, page);
                if(page_open) {
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
            if((bn::keypad::right_pressed() || bn::keypad::a_pressed()) &&
               reader::next_page(*active_source, settings, glyph_width, history, page, next)) {
                page = next; redraw_page = true;
            } else if((bn::keypad::left_pressed() || bn::keypad::b_pressed()) &&
                      reader::previous_page(*active_source, settings, glyph_width, history, next)) {
                page = next; redraw_page = true;
            } else if(bn::keypad::start_pressed()) {
                scene = Scene::SETTINGS; redraw_ui = true;
            } else if(bn::keypad::l_pressed()) {
                reader::TxtSaveFooter footer{page.start_offset, settings};
                file.save_footer(footer);
            } else if(bn::keypad::select_pressed()) {
                epub.close(); file.close(); open_name = nullptr;
                scene = Scene::LIBRARY; redraw_ui = true;
            }
            if(scene == Scene::READER && active_source == &epub && epub.error() != reader::EpubError::NONE) {
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
                reader::layout_page(*active_source, page.start_offset, settings, glyph_width, page);
                redraw_ui = true;
            }
            if(bn::keypad::b_pressed() || bn::keypad::start_pressed()) {
                uint32_t resume_offset = page.start_offset;
                reader::open_page_at(*active_source, resume_offset, settings, glyph_width, history, page);
                scene = Scene::READER; sprites.clear(); redraw_page = true; redraw_ui = false;
            }
        }

        if(redraw_page) { draw_page(painter); redraw_page = false; }
        if(redraw_ui) {
            painter.fill(0); painter.flip_page_later();
            sprites.clear();
            ui.set_center_alignment();
            if(scene == Scene::LIBRARY) {
                add_text(ui, 0, -68, "GBA Reader v0.4.0", sprites);
                if(! storage_ok) add_text(ui, 0, -48, "Supercard SD not ready", sprites);
                else if(! reader::library_count()) add_text(ui, 0, -48, "No TXT/EPUB in root", sprites);
                else if(library_status) add_text(ui, 0, -48, library_status, sprites);
                int first = selected > 1 ? selected - 1 : 0;
                if(first + LIBRARY_VISIBLE_ROWS > reader::library_count())
                    first = reader::library_count() > LIBRARY_VISIBLE_ROWS ?
                            reader::library_count() - LIBRARY_VISIBLE_ROWS : 0;
                for(int i = first; ! library_status && i < reader::library_count() &&
                                   i < first + LIBRARY_VISIBLE_ROWS; ++i) {
                    bn::string<68> label = i == selected ? "> " : "  ";
                    char display_name[LIBRARY_DISPLAY_CHARACTERS * 4 + 1];
                    library_display_name(reader::library_name(i), display_name);
                    label += display_name;
                    add_text(ui, 0, -44 + (i - first) * 16, label.data(), sprites);
                }
                add_text(ui, 0, 68, "UP/DOWN select   A open", sprites);
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
