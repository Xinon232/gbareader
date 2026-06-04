// render.cpp — UI rendering layer
// Step 5l: very light blue background, no bottom mode-swap hint.
//
// Layout (240x160, mixed fonts):
//   y=-64:   "Mode 1" / "Mode 2"             (8x16, top, centered)
//   y=-44:   "Field N/5 - T words"          (8x16, header, centered)
//   y=-20:   <prompt word>                   (16x16 — BIG, centered)
//   y=+12:   <answer word>                   (16x16, only when R is held)
//   y=+64:   "F1:X F2:X F3:X F4:X F5:X"       (8x16, footer, centered)
//
// Background: very light blue (24, 30, 31).
// Text: white.
// Flash on A press: green tint for ~150ms.
// Flash on B press: red   tint for ~150ms.

#include "render.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_sprite_text_generator.h"
#include "bn_format.h"
#include "bn_string.h"

#include "common_variable_8x16_sprite_font.h"
#include "vocab_latin_old_ext_sprite_font.h"
#include "vocab_superfw_cyrillic_font_sprite_font.h"
#include "vocab_dejavu_arabic_font_sprite_font.h"
#include "bn_sprite_items_field_underline.h"

namespace {

constexpr int Y_MODE      = -64;
constexpr int Y_HEADER    = -44;
constexpr int Y_PROMPT    = -20;
constexpr int Y_ANSWER    =  12;
constexpr int Y_FOOTER    =  64;

constexpr int SAVE_X = 116;
constexpr int SAVE_Y = -72;
constexpr int WRAP_MAX_CHARS = 27;
constexpr int WRAP_MAX_LINES = 2;
constexpr int WRAP_LINE_STEP = 12;

constexpr int FLASH_FRAMES = 10;  // ~150ms at 60fps

// Very light blue background. 5-bit RGB: (24, 30, 31).
constexpr int BG_R = 24;
constexpr int BG_G = 30;
constexpr int BG_B = 31;

struct WrappedText {
    char lines[WRAP_MAX_LINES][VOCAB_LINE_MAX];
    int count;
};

int str_len(const char* text)
{
    int len = 0;
    while (text[len] != 0) {
        ++len;
    }
    return len;
}

void copy_slice(char* dest, const char* src, int start, int end)
{
    while (start < end && src[start] == ' ') {
        ++start;
    }
    while (end > start && src[end - 1] == ' ') {
        --end;
    }

    int out = 0;
    while (start < end && out < VOCAB_LINE_MAX - 1) {
        dest[out++] = src[start++];
    }
    dest[out] = 0;
}

WrappedText wrap_text(const char* text)
{
    WrappedText wrapped{};
    int len = str_len(text);
    int pos = 0;

    while (pos < len && wrapped.count < WRAP_MAX_LINES) {
        while (pos < len && text[pos] == ' ') {
            ++pos;
        }
        if (pos >= len) {
            break;
        }

        int remaining_lines = WRAP_MAX_LINES - wrapped.count;
        int remaining_chars = len - pos;
        int take = remaining_chars;

        if (remaining_lines > 1 && remaining_chars > WRAP_MAX_CHARS) {
            int limit = pos + WRAP_MAX_CHARS;
            int break_pos = -1;
            for (int i = limit; i > pos; --i) {
                if (text[i] == ' ') {
                    break_pos = i;
                    break;
                }
            }
            if (break_pos <= pos) {
                break_pos = limit;
            }
            take = break_pos - pos;
        }

        copy_slice(wrapped.lines[wrapped.count], text, pos, pos + take);
        ++wrapped.count;
        pos += take;
    }

    if (wrapped.count == 0) {
        wrapped.lines[0][0] = 0;
    }
    return wrapped;
}

bool decode_utf8_codepoint(const char* text, int& index, unsigned& code)
{
    unsigned char ch = static_cast<unsigned char>(text[index]);
    if (ch < 0x80) {
        code = ch;
        ++index;
        return true;
    }
    if ((ch & 0xE0) == 0xC0 && (text[index + 1] & 0xC0) == 0x80) {
        code = ((ch & 0x1F) << 6) | (static_cast<unsigned char>(text[index + 1]) & 0x3F);
        index += 2;
        return true;
    }
    if ((ch & 0xF0) == 0xE0 && (text[index + 1] & 0xC0) == 0x80 &&
        (text[index + 2] & 0xC0) == 0x80) {
        code = ((ch & 0x0F) << 12) |
               ((static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6) |
               (static_cast<unsigned char>(text[index + 2]) & 0x3F);
        index += 3;
        return true;
    }
    index += 1;
    code = '?';
    return false;
}

enum class FlashcardFontKind {
    LATIN,
    CYRILLIC,
    ARABIC
};

FlashcardFontKind flashcard_font_kind(const char* text)
{
    int i = 0;
    bool saw_cyrillic = false;
    while (text[i] != 0) {
        unsigned code = 0;
        decode_utf8_codepoint(text, i, code);
        if ((code >= 0x0600 && code <= 0x06FF) ||
            (code >= 0xFB50 && code <= 0xFEFF)) {
            return FlashcardFontKind::ARABIC;
        }
        if (code >= 0x0400 && code <= 0x04FF) {
            saw_cyrillic = true;
        }
    }
    return saw_cyrillic ? FlashcardFontKind::CYRILLIC : FlashcardFontKind::LATIN;
}

void generate_wrapped_big(bn::sprite_text_generator& gen, int base_y, const char* text,
                          bn::vector<bn::sprite_ptr, 256>& sprites)
{
    WrappedText wrapped = wrap_text(text);
    int start_y = base_y - ((wrapped.count - 1) * WRAP_LINE_STEP) / 2;
    for (int i = 0; i < wrapped.count; ++i) {
        if (wrapped.lines[i][0] != 0) {
            gen.generate(0, start_y + i * WRAP_LINE_STEP, wrapped.lines[i], sprites);
        }
    }
}

void generate_save_indicator(bn::sprite_text_generator& gen,
                             bn::vector<bn::sprite_ptr, 256>& sprites)
{
    gen.set_right_alignment();
    gen.generate(SAVE_X, SAVE_Y, "save...", sprites);
    gen.set_center_alignment();
}

}  // namespace

Renderer::Renderer()
    : small_gen(common::variable_8x16_sprite_font),
      big_gen(vocab_font::latin_old_ext_sprite_font),
      cyrillic_gen(vocab_superfw_cyrillic_font_sprite_font),
      multilang_gen(vocab_dejavu_arabic_font_sprite_font),
      last_line_idx(-1),
      last_field(0),
      last_active_side(State::SIDE_A),
      last_show_answer(false),
      last_field_is_empty(false),
      last_counts{-1, -1, -1, -1, -1},
      saving_visible(false),
      last_saving_visible(false),
      flash_timer_frames(0),
      flash_color(0)
{
    small_gen.set_center_alignment();
    big_gen.set_center_alignment();
    cyrillic_gen.set_center_alignment();
    multilang_gen.set_center_alignment();
}
Renderer::~Renderer() {
}

void Renderer::reset() {
    last_line_idx = -1;
    last_field = 0;
    last_active_side = State::SIDE_A;
    last_show_answer = false;
    last_field_is_empty = false;
    for (int i = 0; i < 5; i++) last_counts[i] = -1;
    last_saving_visible = !saving_visible;
    text_sprites.clear();
    flash_timer_frames = 0;
    flash_color = 0;
}

void Renderer::set_saving(bool saving) {
    if (saving_visible != saving) {
        saving_visible = saving;
        last_saving_visible = !saving;
    }
}

void Renderer::update(const VocabFile& vf, int current_line_idx, int current_field,
                      const LineBuf& current,
                      State::Side active_side, bool show_answer,
                      bool field_is_empty)
{
    // Background: light blue, or flash color if active.
    if (flash_timer_frames > 0) {
        flash_timer_frames--;
        if (flash_color == 1) {
            bn::bg_palettes::set_transparent_color(bn::color(0, 31, 0));  // green
        } else if (flash_color == 2) {
            bn::bg_palettes::set_transparent_color(bn::color(31, 0, 0));  // red
        } else {
            bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
        }
    } else {
        bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    }

    // Detect what changed.
    bool counts_changed = false;
    for (int i = 0; i < 5; i++) {
        if (vf.field_counts[i] != (uint16_t)last_counts[i]) {
            counts_changed = true;
            break;
        }
    }

    if (current_line_idx != last_line_idx ||
        current_field != last_field ||
        active_side != last_active_side ||
        show_answer != last_show_answer ||
        field_is_empty != last_field_is_empty ||
        saving_visible != last_saving_visible ||
        counts_changed) {
        render_full(vf, current_line_idx, current_field, current,
                    active_side, show_answer, field_is_empty);
        last_line_idx = current_line_idx;
        last_field = current_field;
        last_active_side = active_side;
        last_show_answer = show_answer;
        last_field_is_empty = field_is_empty;
        last_saving_visible = saving_visible;
        for (int i = 0; i < 5; i++) last_counts[i] = vf.field_counts[i];
    }
}

void Renderer::update_browser(const State& state)
{
    bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    text_sprites.clear();

    small_gen.generate(0, -64, "Select TXT file", text_sprites);
    small_gen.generate(0, -44, "A load   B cancel", text_sprites);
    if (saving_visible) {
        generate_save_indicator(small_gen, text_sprites);
    }

    int top = state.browse_top();
    int selected = state.browse_index();
    if (selected < top) {
        top = selected;
    }
    if (selected >= top + 5) {
        top = selected - 4;
    }

    for (int row = 0; row < 5; row++) {
        int file_index = top + row;
        const char* name = state.filename(file_index);
        if (!name) {
            continue;
        }
        bn::string<40> line;
        if (file_index == selected) {
            line = bn::format<40>("> {}", name);
        } else {
            line = bn::format<40>("  {}", name);
        }
        small_gen.generate(0, -16 + row * 18, line, text_sprites);
    }
}

void Renderer::update_shuffle_confirm(int current_field)
{
    bn::bg_palettes::set_transparent_color(bn::color(BG_R, BG_G, BG_B));
    text_sprites.clear();

    small_gen.generate(0, -36, "Shuffle items", text_sprites);
    bn::string<32> line = bn::format<32>("in box {}?", current_field);
    small_gen.generate(0, -14, line, text_sprites);
    small_gen.generate(0, 20, "A yes   B no", text_sprites);
}

void Renderer::render_full(const VocabFile& vf, int current_line_idx, int current_field,
                           const LineBuf& current,
                           State::Side active_side, bool show_answer,
                           bool field_is_empty)
{
    text_sprites.clear();

    // Mode indicator (very top, centered): "Mode N"
    {
        bn::string<8> mode_str = bn::format<8>("Mode {}",
            (active_side == State::SIDE_A) ? 1 : 2);
        small_gen.generate(0, Y_MODE, mode_str, text_sprites);
    }

    // Header: "Field N/5 - T words" (centered, 8x16)
    if (current_line_idx >= 0 && current_line_idx < vf.line_count) {
        int total = 0;
        for (int i = 0; i < 5; i++) total += vf.field_counts[i];
        bn::string<48> header = bn::format<48>(
            "Field {}/{} - {} words",
            current_field, 5, total);
        small_gen.generate(0, Y_HEADER, header, text_sprites);
    } else {
        bn::string<16> header = "No vocab";
        small_gen.generate(0, Y_HEADER, header, text_sprites);
    }

    // Prompt: the active side of the current word. 16x16 — BIG.
    if (field_is_empty) {
        bn::string<8> empty_str = "EMPTY";
        big_gen.generate(0, Y_PROMPT, empty_str, text_sprites);
    } else {
        const char* prompt = (active_side == State::SIDE_A) ? current.a : current.b;
        if (prompt[0] != 0) {
            FlashcardFontKind kind = flashcard_font_kind(prompt);
            bn::sprite_text_generator& gen = (kind == FlashcardFontKind::ARABIC) ? multilang_gen :
                                             ((kind == FlashcardFontKind::CYRILLIC) ? cyrillic_gen : big_gen);
            generate_wrapped_big(gen, Y_PROMPT, prompt, text_sprites);
        }
    }

    // Answer: the other side, only when R held. 16x16 — BIG.
    if (show_answer && !field_is_empty) {
        const char* answer = (active_side == State::SIDE_A) ? current.b : current.a;
        if (answer[0] != 0) {
            FlashcardFontKind kind = flashcard_font_kind(answer);
            bn::sprite_text_generator& gen = (kind == FlashcardFontKind::ARABIC) ? multilang_gen :
                                             ((kind == FlashcardFontKind::CYRILLIC) ? cyrillic_gen : big_gen);
            generate_wrapped_big(gen, Y_ANSWER, answer, text_sprites);
        }
    }

    // Footer: one label per field. Underline the box the user is browsing.
    if (saving_visible) {
        generate_save_indicator(small_gen, text_sprites);
    }

    static constexpr int FOOTER_X[5] = { -96, -48, 0, 48, 96 };
    for (int i = 0; i < 5; i++) {
        bn::string<16> field_label = bn::format<16>("F{}:{}", i + 1, vf.field_counts[i]);
        small_gen.generate(FOOTER_X[i], Y_FOOTER, field_label, text_sprites);
    }

    int active_field_index = current_field - 1;
    if (active_field_index >= 0 && active_field_index < 5) {
        int x = FOOTER_X[active_field_index];
        for (int i = 0; i < 4; i++) {
            text_sprites.push_back(
                bn::sprite_items::field_underline.create_sprite(x - 12 + i * 8, Y_FOOTER + 12));
        }
    }
}

void Renderer::flash_green() {
    flash_timer_frames = FLASH_FRAMES;
    flash_color = 1;
    bn::bg_palettes::set_transparent_color(bn::color(0, 31, 0));
}

void Renderer::flash_red() {
    flash_timer_frames = FLASH_FRAMES;
    flash_color = 2;
    bn::bg_palettes::set_transparent_color(bn::color(31, 0, 0));
}
