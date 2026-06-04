// state.h — Scene state machine
// Step 5d: revised per user feedback.
//
// State machine:
//   TRAIN  -- the main training screen
//   BROWSE -- file browser (Step 6, stub for now)
//
// Within TRAIN, we track:
//   - current_line_idx: which vocab entry is being asked
//   - direction_mode: 1 (show A, recall B),
//                      2 (show B, recall A),
//                      3 (alternate per word: A→B→A→B→...)
//   - current_field: the field (1..5) the user is browsing
//   - alternation_phase: in mode 3, this is the "next" side to show
//   - undo_pending: true if there's a most-recent A/B press that can
//                   still be undone. Disabled once the user navigates
//                   to a different word (D-pad, or A/B on another word).
//
// Button mappings:
//   A: correct recall → vocab_advance + flash_green
//   B: wrong recall   → vocab_reset   + flash_red
//   R: held           → reveal the answer (the other side of the pair)
//   L: tap            → cycle direction_mode 1→2→3→1
//   D-pad Left/Right: switch between boxes (fields), including empty ones
//   D-pad Up:         undo the most recent A/B press (one-shot).
//   D-pad Down:       ask to shuffle only the current box; A confirms, B cancels.
//   START: save .txt back to disk (Step 6 stub: clears dirty flags)
//   SELECT: file browser (Step 6, stub for now)
//
// Direction mode behavior:
//   mode 1: show A (column 1), recall B (column 2). Prompt = word A.
//   mode 2: show B (column 2), recall A. Prompt = word B.
//   mode 3: alternate per word. alternation_phase_ tracks which side
//           is the prompt for the NEXT word. Each A/B press toggles it.
//
// Undo semantics (simplified):
//   - After A or B press, undo_pending_ = true. The previous
//     field-of-current-word is stored.
//   - D-pad Up while undo_pending_: restore the field, clear
//     undo_pending_. Jumps the display back to the current word.
//   - Any action that changes which word is on screen (D-pad L/R
//     to a new box, another A/B press, SELECT to BROWSE) clears
//     undo_pending_ so Up does nothing.
//
// The renderer is told which side to display via the active_side()
// function. The "answer" is the other side; show_answer() is true
// when R is held (rendering shows the other side too).

#pragma once

#include "vocab.h"

class State {
public:
    enum FlashRequest { FLASH_NONE, FLASH_GREEN, FLASH_RED };
    enum Side { SIDE_A, SIDE_B };

    // What the input layer reports each frame.
    struct InputState {
        bool a_pressed = false;
        bool b_pressed = false;
        bool r_held = false;
        bool l_pressed = false;
        bool start_pressed = false;
        bool select_pressed = false;
        bool left_pressed = false;
        bool right_pressed = false;
        bool up_pressed = false;     // undo most-recent A/B
        bool down_pressed = false;
    };

    State();

    bool update(VocabFile& vf, const InputState& in);

    int current_line_idx() const { return current_line_idx_; }
    int direction_mode() const { return direction_mode_; }
    int current_field() const { return current_field_; }
    bool show_answer() const { return show_answer_; }
    int scene() const { return scene_; }
    bool undo_pending() const { return undo_pending_; }
    bool shuffle_confirm_active() const { return scene_ == 2; }
    bool feedback_active() const { return scene_ == 3; }

    // After saving to SD the file is rewritten grouped by field and then
    // re-opened, so numeric line indexes can point at different words.
    // Restore to a precomputed line index in the new grouped order.
    bool restore_current_line_index(const VocabFile& vf, int line_idx);

    bool current_field_is_empty(const VocabFile& vf) const;

    Side active_side() const;
    Side answer_side() const {
        return active_side() == SIDE_A ? SIDE_B : SIDE_A;
    }

    // Test/setup helpers.
    void debug_set_line(int idx) { current_line_idx_ = idx; }
    void debug_set_field(int f) { current_field_ = f; }
    void debug_set_direction(int m) { direction_mode_ = m; }
    void debug_set_alternation(Side s) { alternation_phase_ = s; }
    void debug_set_undo(bool v) { undo_pending_ = v; }

    int browse_index() const { return browse_index_; }
    int browse_top() const { return browse_top_; }
    bool load_request_pending() const { return load_request_pending_; }
    const char* consume_load_request();

    FlashRequest consume_flash();

    int file_count() const;
    const char* filename(int i) const;

private:
    int current_line_idx_;
    int direction_mode_;
    int current_field_;
    Side alternation_phase_;
    bool show_answer_;
    int scene_;
    FlashRequest flash_request_;
    int browse_index_;
    int browse_top_;
    bool load_request_pending_;
    int load_request_index_;
    int last_line_by_field_[5];
    uint32_t shuffle_seed_;
    int feedback_frames_left_;

    // Feedback scene: after A/B, keep the pressed card visible with
    // answer shown during the green/red flash. Only after the flash
    // expires do we advance to the next word and toggle alternation.
    int feedback_line_idx_;
    bool feedback_toggle_alternation_;

    // Undo state. Single-shot. Stored only if the most recent A/B
    // press should be undoable. For B in field 1 the field does not
    // change, but undo still returns to the word that was advanced past.
    // Cleared by any action that
    // changes which field the user is browsing (D-pad L/R to a new
    // box), or by a successful undo. The undo restores the field of
    // the changed word and jumps the display back to that word so
    // the user can re-decide.
    //
    // undo_field_at_press: the current_field_ value at the moment
    // the user pressed A/B. The undo will fire only if the user is
    // still browsing this field (matches "only if user stays at
    // current box").
    bool undo_pending_;
    int undo_line_idx_;
    uint8_t undo_old_field_;
    uint8_t undo_field_at_press_;

    void clear_undo() { undo_pending_ = false; }

    void remember_current_line_for_field(const VocabFile& vf);
    void restore_current_line_for_field(const VocabFile& vf);
    void set_current_line_for_field(const VocabFile& vf, int line_idx);
    void find_next_word_in_field(const VocabFile& vf);
    void finish_feedback(VocabFile& vf);
    void jump_to_next_field(const VocabFile& vf);
    void jump_to_prev_field(const VocabFile& vf);
    void shuffle_current_field(VocabFile& vf);
};

