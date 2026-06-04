// render.h — UI rendering layer
// Step 5i: single 8x16 font (butano's common_variable_8x16) for all text.

#pragma once

#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"

#include "vocab.h"
#include "state.h"  // for State::Side enum

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Update the display with the current word and field counts.
    // Called every frame.
    void update(const VocabFile& vf, int current_line_idx, int current_field,
                const LineBuf& current,
                State::Side active_side,    // which side of the pair to show
                bool show_answer,          // R held → also show the other side
                bool field_is_empty);      // current box has no words

    void update_browser(const State& state);
    void update_shuffle_confirm(int current_field);

    // Show/hide the short save indicator in the top-right corner.
    void set_saving(bool saving);

    // Trigger a flash. flash_green() = A press. flash_red() = B press.
    void flash_green();
    void flash_red();

    void reset();

private:
    // Two text generators: one for the small UI text, one for
    // the flashcard words using the SuperFW/UnSCI 8x16 font.
    bn::sprite_text_generator small_gen;
    bn::sprite_text_generator big_gen;
    bn::sprite_text_generator cyrillic_gen;
    bn::sprite_text_generator multilang_gen;
    bn::vector<bn::sprite_ptr, 256> text_sprites;

    int last_line_idx;
    int last_field;
    State::Side last_active_side;
    bool last_show_answer;
    bool last_field_is_empty;
    int last_counts[5];
    bool saving_visible;
    bool last_saving_visible;
    int flash_timer_frames;
    int flash_color;  // 0 = none, 1 = green, 2 = red

    void render_full(const VocabFile& vf, int current_line_idx, int current_field,
                     const LineBuf& current,
                     State::Side active_side, bool show_answer,
                     bool field_is_empty);
};
