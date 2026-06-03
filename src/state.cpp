// state.cpp — Scene state machine
// Step 5d: single-shot undo (D-pad Up).

#include "state.h"
#include "vocab_file_io.h"

#include <cstdio>

constexpr int SCENE_TRAIN = 0;
constexpr int SCENE_BROWSE = 1;

State::State()
    : current_line_idx_(0),
      direction_mode_(1),
      current_field_(1),
      alternation_phase_(SIDE_A),
      show_answer_(false),
      scene_(SCENE_TRAIN),
      flash_request_(FLASH_NONE),
      browse_index_(0),
      browse_top_(0),
      load_request_pending_(false),
      load_request_index_(0),
      undo_pending_(false),
      undo_line_idx_(0),
      undo_old_field_(1),
      undo_field_at_press_(1)
{
}

int State::file_count() const { return vocab_file_count(); }

const char* State::filename(int i) const {
    return vocab_file_name(i);
}

const char* State::consume_load_request() {
    if (!load_request_pending_) return nullptr;
    load_request_pending_ = false;
    return filename(load_request_index_);
}

bool State::restore_current_line_index(const VocabFile& vf, int line_idx)
{
    if (line_idx < 0 || line_idx >= vf.line_count) {
        return false;
    }
    current_line_idx_ = line_idx;
    return true;
}

State::Side State::active_side() const {
    if (direction_mode_ == 2) return SIDE_B;
    if (direction_mode_ == 3) return alternation_phase_;
    return SIDE_A;
}

bool State::current_field_is_empty(const VocabFile& vf) const {
    for (int i = 0; i < vf.line_count; i++) {
        if (vf.field[i] == (uint8_t)current_field_) return false;
    }
    return true;
}

static bool find_word_in_field(const VocabFile& vf, int start_idx,
                              int target_field, int& out_idx)
{
    int n = vf.line_count;
    if (n == 0) return false;
    for (int step = 1; step <= n; step++) {
        int i = (start_idx + step) % n;
        if (vf.field[i] == target_field) {
            out_idx = i;
            return true;
        }
    }
    return false;
}

static bool find_word_in_field_back(const VocabFile& vf, int start_idx,
                                   int target_field, int& out_idx)
{
    int n = vf.line_count;
    if (n == 0) return false;
    for (int step = 1; step <= n; step++) {
        int i = (start_idx - step + n) % n;
        if (vf.field[i] == target_field) {
            out_idx = i;
            return true;
        }
    }
    return false;
}

static int wrap_field(int field)
{
    while (field < 1) field += 5;
    while (field > 5) field -= 5;
    return field;
}

void State::find_next_word_in_field(const VocabFile& vf)
{
    int new_idx = current_line_idx_;
    if (find_word_in_field(vf, current_line_idx_, current_field_, new_idx)) {
        current_line_idx_ = new_idx;
    }
}

void State::jump_to_next_field(const VocabFile& vf)
{
    current_field_ = wrap_field(current_field_ + 1);

    int new_idx = current_line_idx_;
    if (find_word_in_field(vf, current_line_idx_, current_field_, new_idx)) {
        current_line_idx_ = new_idx;
    }
}

void State::jump_to_prev_field(const VocabFile& vf)
{
    current_field_ = wrap_field(current_field_ - 1);

    int new_idx = current_line_idx_;
    if (find_word_in_field_back(vf, current_line_idx_, current_field_, new_idx)) {
        current_line_idx_ = new_idx;
    }
}

bool State::update(VocabFile& vf, const InputState& in)
{
    if (scene_ == SCENE_TRAIN) {
        show_answer_ = in.r_held;

        if (in.l_pressed) {
            direction_mode_++;
            if (direction_mode_ > 3) direction_mode_ = 1;
        }

        // D-pad L/R switches boxes. This is a "navigate away" action,
        // so it clears any pending undo.
        if (in.right_pressed) {
            jump_to_next_field(vf);
            clear_undo();
        }
        if (in.left_pressed) {
            jump_to_prev_field(vf);
            clear_undo();
        }

        // D-pad Up: undo the most recent A/B press, if one is pending.
        // The undo works as long as the user is still browsing the
        // same field they were when they pressed A/B. (Per spec:
        // "only if user stays at current box.") It jumps the display
        // back to the changed word so the user can re-decide.
        if (in.up_pressed && undo_pending_ &&
            current_field_ == undo_field_at_press_) {
            uint8_t new_field = vf.field[undo_line_idx_];
            vf.field[undo_line_idx_] = undo_old_field_;
            if (new_field != undo_old_field_) {
                vf.field_counts[new_field - 1]--;
                vf.field_counts[undo_old_field_ - 1]++;
            }
            current_line_idx_ = undo_line_idx_;  // jump back
            clear_undo();
            flash_request_ = FLASH_NONE;  // no flash on undo
        }

        bool in_empty_box = current_field_is_empty(vf);

        if (in.a_pressed && vf.line_count > 0 && !in_empty_box) {
            int line = current_line_idx_;
            uint8_t old_field = vf.field[line];
            vocab_advance(vf, line);
            uint8_t new_field = vf.field[line];
            // Only arm the one-shot undo if the field actually
            // changed. Record the field the user was browsing at
            // the time of the press so we can verify they're still
            // there when they press Up.
            if (old_field != new_field) {
                undo_pending_ = true;
                undo_line_idx_ = line;
                undo_old_field_ = old_field;
                undo_field_at_press_ = (uint8_t)current_field_;
            } else {
                clear_undo();
            }
            flash_request_ = FLASH_GREEN;
            find_next_word_in_field(vf);
            if (direction_mode_ == 3) {
                alternation_phase_ = (alternation_phase_ == SIDE_A) ? SIDE_B : SIDE_A;
            }
        }

        if (in.b_pressed && vf.line_count > 0 && !in_empty_box) {
            int line = current_line_idx_;
            uint8_t old_field = vf.field[line];
            vocab_reset(vf, line);
            // B press is undoable even when the word was already in field 1:
            // undo then means "return to the word I just marked wrong".
            undo_pending_ = true;
            undo_line_idx_ = line;
            undo_old_field_ = old_field;
            undo_field_at_press_ = (uint8_t)current_field_;
            flash_request_ = FLASH_RED;
            find_next_word_in_field(vf);
            if (direction_mode_ == 3) {
                alternation_phase_ = (alternation_phase_ == SIDE_A) ? SIDE_B : SIDE_A;
            }
        }

        if (in.start_pressed) {
            vocab_clear_dirty(vf);
        }

        if (in.select_pressed) {
            clear_undo();
            scene_ = SCENE_BROWSE;
        }
    } else if (scene_ == SCENE_BROWSE) {
        if (in.b_pressed) {
            load_request_pending_ = false;
            scene_ = SCENE_TRAIN;
        }
        if (in.a_pressed) {
            load_request_pending_ = true;
            load_request_index_ = browse_index_;
            scene_ = SCENE_TRAIN;
        }
        if (in.up_pressed && browse_index_ > 0) {
            browse_index_--;
            if (browse_index_ < browse_top_) browse_top_ = browse_index_;
        }
        if (in.down_pressed && browse_index_ < file_count() - 1) {
            browse_index_++;
        }
        if (in.left_pressed) {
            browse_index_ -= 5;
            if (browse_index_ < 0) browse_index_ = 0;
            if (browse_top_ > browse_index_) browse_top_ = browse_index_;
        }
        if (in.right_pressed) {
            browse_index_ += 5;
            if (browse_index_ >= file_count()) browse_index_ = file_count() - 1;
        }
    }

    return true;
}

State::FlashRequest State::consume_flash() {
    FlashRequest r = flash_request_;
    flash_request_ = FLASH_NONE;
    return r;
}
