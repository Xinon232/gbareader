// GBA Vocab Trainer — main entry
// Step 5c: light blue background, single-word prompt, R reveals
// answer, D-pad L/R switches boxes, L cycles 3 direction modes.

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_keypad.h"

#include "vocab.h"
#include "sav.h"
#include "render.h"
#include "state.h"
#include "vocab_file_io.h"

#include "common_variable_8x16_sprite_font.h"

BN_DATA_EWRAM_BSS VocabFile g_vocab_file;

BN_DATA_EWRAM_BSS char g_builtin_vocab[VOCAB_FILE_BUFFER_LEN];
BN_DATA_EWRAM_BSS char g_export_buffer[VOCAB_EXPORT_BUFFER_LEN];
BN_DATA_EWRAM_BSS int g_builtin_vocab_used = 0;
BN_DATA_EWRAM_BSS int g_export_buffer_used = 0;

static void step3_savestate_init()
{
    Savestate sav;
    bool sav_valid = sav_load(sav);
    if (sav_valid) {
        sav_save(sav);
    } else {
        Savestate defaults;
        memset(&defaults, 0, sizeof(defaults));
        defaults.last_field = 1;
        defaults.last_line = 0;
        const char* default_name = "builtin.txt";
        for (int i = 0; default_name[i] && i < SAV_FILENAME_MAX - 1; i++) {
            defaults.filename[i] = (uint8_t)default_name[i];
        }
        defaults.filename[SAV_FILENAME_MAX - 1] = 0;
        sav_save(defaults);
    }
}

static void load_builtin_vocab()
{
    vocab_file_load("builtin.txt", g_vocab_file, g_builtin_vocab,
                    VOCAB_FILE_BUFFER_LEN, g_builtin_vocab_used);
}

static bool load_selected_vocab(const char* filename)
{
    return vocab_file_load(filename, g_vocab_file, g_builtin_vocab,
                           VOCAB_FILE_BUFFER_LEN, g_builtin_vocab_used);
}

static int grouped_save_index_for_line(const VocabFile& vf, int old_idx)
{
    if (old_idx < 0 || old_idx >= vf.line_count) {
        return -1;
    }

    uint8_t field = vf.field[old_idx];
    if (field < 1 || field > 5) {
        return -1;
    }

    int new_idx = 0;
    for (int i = 0; i < vf.line_count; ++i) {
        uint8_t candidate_field = vf.field[i];
        if (candidate_field < field || (candidate_field == field && i < old_idx)) {
            ++new_idx;
        }
    }
    return new_idx;
}

static State::InputState read_input()
{
    State::InputState in;
    in.a_pressed       = bn::keypad::a_pressed();
    in.b_pressed       = bn::keypad::b_pressed();
    in.r_held          = bn::keypad::r_held();
    in.l_pressed       = bn::keypad::l_pressed();
    in.start_pressed   = bn::keypad::start_pressed();
    in.select_pressed  = bn::keypad::select_pressed();
    in.left_pressed    = bn::keypad::left_pressed();
    in.right_pressed   = bn::keypad::right_pressed();
    in.up_pressed      = bn::keypad::up_pressed();
    in.down_pressed    = bn::keypad::down_pressed();
    return in;
}

static void render_current_frame(Renderer& renderer, State& state)
{
    if (state.scene() == 1) {
        renderer.update_browser(state);
        return;
    }
    if (state.shuffle_confirm_active()) {
        renderer.update_shuffle_confirm(state.current_field());
        return;
    }

    int idx = state.current_line_idx();
    if (idx < 0 || idx >= g_vocab_file.line_count) idx = 0;

    LineBuf current;
    bool field_empty = !state.feedback_active() && state.current_field_is_empty(g_vocab_file);
    if (vocab_file_show(g_vocab_file, g_builtin_vocab, g_builtin_vocab_used,
                        idx, current) || field_empty) {
        renderer.update(g_vocab_file, idx, state.current_field(),
                        current, state.active_side(), state.direction_mode() == 3,
                        state.show_answer(), field_empty);
    }
}

int main()
{
    bn::core::init();

    step3_savestate_init();
    vocab_file_init();
    load_builtin_vocab();

    Renderer renderer;
    State state;
    int last_scene = state.scene();

    while(true)
    {
        State::InputState in = read_input();
        state.update(g_vocab_file, in);

        if (in.start_pressed && state.scene() == 0) {
            int grouped_idx_after_save = -1;
            int idx_before_save = state.current_line_idx();
            if (vocab_file_loaded_from_sd() &&
                idx_before_save >= 0 && idx_before_save < g_vocab_file.line_count &&
                !state.current_field_is_empty(g_vocab_file)) {
                grouped_idx_after_save = grouped_save_index_for_line(g_vocab_file, idx_before_save);
            }

            renderer.set_saving(true);
            render_current_frame(renderer, state);
            bn::core::update();

            bool saved = vocab_file_save_grouped(g_vocab_file, g_builtin_vocab, g_builtin_vocab_used,
                                                 g_export_buffer, VOCAB_EXPORT_BUFFER_LEN,
                                                 g_export_buffer_used);
            if (saved && grouped_idx_after_save >= 0) {
                state.restore_current_line_index(g_vocab_file, grouped_idx_after_save);
            }

            renderer.set_saving(false);
            renderer.reset();
        }

        if (state.scene() != last_scene) {
            renderer.reset();
            last_scene = state.scene();
        }

        if (state.load_request_pending()) {
            const char* filename = state.consume_load_request();
            if (load_selected_vocab(filename)) {
                renderer.reset();
            }
        }

        switch (state.consume_flash()) {
            case State::FLASH_GREEN: renderer.flash_green(); break;
            case State::FLASH_RED:   renderer.flash_red(); break;
            case State::FLASH_NONE:
            default: break;
        }

        render_current_frame(renderer, state);

        bn::core::update();
    }
}
