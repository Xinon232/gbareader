// test_state.cpp — host-side state machine test
// Each test is independent: rebuilds VocabFile and State from scratch.
// This avoids the tangled state issues from the earlier monolithic
// test where a single A press's effect would bleed into later checks.

#include "vocab.h"
#include "state.h"

#include <cstdio>
#include <cstring>

// Helpers ----------------------------------------------------------------

static void load_n_pairs(VocabFile& vf, int n)
{
    vf.reset();
    char buf[8192];
    int len = 0;
    for (int i = 0; i < n; i++) {
        int written = snprintf(buf + len, sizeof(buf) - len,
                               "w%03d\tt%03d\n", i, i);
        if (written <= 0) break;
        len += written;
    }
    vocab_open(vf, buf, len);
}

// Test 1: mode cycling 1→2→3→1
static int test_modes()
{
    printf("[1] Direction modes\n");
    VocabFile vf; load_n_pairs(vf, 5);
    State state;
    State::InputState in;
    if (state.active_side() != State::SIDE_A) return 1;
    in = State::InputState{};
    in.l_pressed = true; state.update(vf, in);  // 1→2
    if (state.active_side() != State::SIDE_B) return 1;
    in = State::InputState{};
    in.l_pressed = true; state.update(vf, in);  // 2→3
    if (state.active_side() != State::SIDE_A) return 1;  // phase 0
    in = State::InputState{};
    in.a_pressed = true; state.update(vf, in);  // phase toggle
    if (state.active_side() != State::SIDE_B) return 1;
    in = State::InputState{};
    in.l_pressed = true; state.update(vf, in);  // 3→1
    if (state.active_side() != State::SIDE_A) return 1;
    printf("    OK\n");
    return 0;
}

// Test 2: R held → show_answer
static int test_r_held()
{
    printf("[2] R held\n");
    VocabFile vf; load_n_pairs(vf, 5);
    State state;
    State::InputState in;
    in.r_held = true; state.update(vf, in);
    if (!state.show_answer()) return 1;
    in = State::InputState{};
    in.r_held = false; state.update(vf, in);
    if (state.show_answer()) return 1;
    printf("    OK\n");
    return 0;
}

// Test 3: B press keeps current_field_ unchanged (the bug fix)
static int test_b_keeps_field()
{
    printf("[3] B press keeps current_field\n");
    VocabFile vf; load_n_pairs(vf, 5);
    State state;
    // Move word 0 to field 2 manually.
    vocab_advance(vf, 0);  // 1→2
    state.debug_set_line(0);
    state.debug_set_field(1);
    int saved_field = state.current_field();
    State::InputState in;
    in.b_pressed = true; state.update(vf, in);
    if (state.current_field() != saved_field) {
        printf("    FAIL: B changed field %d → %d\n", saved_field, state.current_field());
        return 1;
    }
    if (vf.field[0] != 1) {
        printf("    FAIL: word[0] should be at field 1, got %u\n", vf.field[0]);
        return 1;
    }
    printf("    OK\n");
    return 0;
}

// Test 4: D-pad L/R switches boxes, including empty fields
static int test_dpad_box_switch()
{
    printf("[4] D-pad L/R switches boxes including empty\n");
    VocabFile vf; load_n_pairs(vf, 10);
    // Move some words to field 3, leaving field 2 empty.
    vocab_advance(vf, 0); vocab_advance(vf, 0);
    vocab_advance(vf, 1); vocab_advance(vf, 1);
    State state;
    state.debug_set_field(1);
    State::InputState in;
    in.right_pressed = true; state.update(vf, in);
    // 1 → 2, even though field 2 is empty.
    if (state.current_field() != 2) {
        printf("    FAIL: expected to land at empty field 2, got %d\n", state.current_field());
        return 1;
    }
    if (!state.current_field_is_empty(vf)) {
        printf("    FAIL: field 2 should be empty\n");
        return 1;
    }
    in = State::InputState{};
    in.right_pressed = true; state.update(vf, in);
    if (state.current_field() != 3) {
        printf("    FAIL: expected to land at field 3, got %d\n", state.current_field());
        return 1;
    }
    // Reset in before left_pressed.
    in = State::InputState{};
    in.left_pressed = true; state.update(vf, in);
    if (state.current_field() != 2) {
        printf("    FAIL: expected to land back at empty field 2, got %d\n", state.current_field());
        return 1;
    }
    printf("    OK\n");
    return 0;
}

// Test 5: One-shot undo: A press arms it, D-pad Up fires it, second
// Up is no-op. User must be in the same field the word went to.
// Each subtest resets state.
static int test_undo_one_shot()
{
    printf("[5] One-shot undo\n");

    // Sub-test A: Up restores when in same field.
    {
        VocabFile vf; load_n_pairs(vf, 5);
        State state;
        state.debug_set_line(0);
        state.debug_set_field(1);
        State::InputState in;
        in.a_pressed = true; state.update(vf, in);
        if (!state.undo_pending()) { printf("    FA: A should arm\n"); return 1; }
        if (vf.field[0] != 2) { printf("    FA: word[0]=%u, expect 2\n", vf.field[0]); return 1; }
        in = State::InputState{};
        in.up_pressed = true; state.update(vf, in);
        if (vf.field[0] != 1) { printf("    FA: Up didn't restore, got %u\n", vf.field[0]); return 1; }
        if (state.undo_pending()) { printf("    FA: undo should be cleared\n"); return 1; }
    }

    // Sub-test B: D-pad Right clears pending undo.
    {
        VocabFile vf; load_n_pairs(vf, 5);
        State state;
        state.debug_set_line(0);
        state.debug_set_field(1);
        State::InputState in;
        in.a_pressed = true; state.update(vf, in);
        if (!state.undo_pending()) { printf("    FB: A should arm\n"); return 1; }
        in = State::InputState{};
        in.right_pressed = true; state.update(vf, in);
        if (state.undo_pending()) { printf("    FB: D-pad Right should clear undo\n"); return 1; }
    }

    // Sub-test C: Second A press on same word overwrites the undo
    // (only the most recent A/B is undoable).
    {
        VocabFile vf; load_n_pairs(vf, 5);
        State state;
        state.debug_set_line(0);
        state.debug_set_field(1);
        State::InputState in;
        in.a_pressed = true; state.update(vf, in);  // 1→2
        if (vf.field[0] != 2) { printf("    FC: expect 2, got %u\n", vf.field[0]); return 1; }
        // Find_next_word_in_field moved us to a different line. We
        // want to test undoing a SECOND A press on the SAME word.
        // So we manually put line back to 0.
        state.debug_set_line(0);
        in = State::InputState{};
        in.a_pressed = true; state.update(vf, in);  // 2→3
        if (vf.field[0] != 3) { printf("    FC: expect 3, got %u\n", vf.field[0]); return 1; }
        if (!state.undo_pending()) { printf("    FC: undo should be armed\n"); return 1; }
        // Undo once: should restore to 2 (only 1 step).
        in = State::InputState{};
        in.up_pressed = true; state.update(vf, in);
        if (vf.field[0] != 2) { printf("    FC: undo only restores 1 step, got %u (expect 2)\n", vf.field[0]); return 1; }
        if (state.undo_pending()) { printf("    FC: undo should be cleared\n"); return 1; }
    }

    printf("    OK\n");
    return 0;
}

// Test 6: D-pad L/R clears pending undo
static int test_dpad_clears_undo()
{
    printf("[6] D-pad L/R clears pending undo\n");
    VocabFile vf; load_n_pairs(vf, 5);
    State state;
    state.debug_set_line(0);
    state.debug_set_field(1);
    State::InputState in;
    in.a_pressed = true; state.update(vf, in);
    if (!state.undo_pending()) return 1;
    in = State::InputState{};
    in.right_pressed = true; state.update(vf, in);
    if (state.undo_pending()) {
        printf("    FAIL: D-pad Right should clear undo\n");
        return 1;
    }
    printf("    OK\n");
    return 0;
}

// Test 7: SELECT → BROWSE, navigate with Up/Down, A requests load, B cancels
static int test_browse()
{
    printf("[7] BROWSE\n");
    VocabFile vf; load_n_pairs(vf, 5);
    State state;
    State::InputState in;
    in.select_pressed = true; state.update(vf, in);
    if (state.scene() != 1) return 1;
    if (state.browse_index() != 0) return 1;
    in = State::InputState{};
    in.down_pressed = true; state.update(vf, in);
    if (state.browse_index() != 1) return 1;
    in = State::InputState{};
    in.down_pressed = true; state.update(vf, in);
    if (state.browse_index() != 2) return 1;
    in = State::InputState{};
    in.down_pressed = true; state.update(vf, in);
    if (state.browse_index() != 2) return 1;  // clamped
    in = State::InputState{};
    in.up_pressed = true; state.update(vf, in);
    if (state.browse_index() != 1) return 1;
    in = State::InputState{};
    in.a_pressed = true; state.update(vf, in);
    if (state.scene() != 0) return 1;
    if (!state.load_request_pending()) return 1;
    if (strcmp(state.consume_load_request(), "NL-DE-5000.txt") != 0) return 1;
    if (state.load_request_pending()) return 1;

    in = State::InputState{};
    in.select_pressed = true; state.update(vf, in);
    in = State::InputState{};
    in.b_pressed = true; state.update(vf, in);
    if (state.scene() != 0) return 1;
    if (state.load_request_pending()) return 1;
    printf("    OK\n");
    return 0;
}

// Test 8: Empty box detection
static int test_empty_box()
{
    printf("[8] Empty box\n");
    VocabFile vf; load_n_pairs(vf, 5);
    State state;
    if (state.current_field_is_empty(vf)) return 1;
    state.debug_set_field(4);
    if (!state.current_field_is_empty(vf)) return 1;
    state.debug_set_field(1);
    if (state.current_field_is_empty(vf)) return 1;
    printf("    OK\n");
    return 0;
}

// Test 9: A/B in empty box is no-op
static int test_empty_box_noop()
{
    printf("[9] A/B in empty box is no-op\n");
    VocabFile vf; load_n_pairs(vf, 5);
    State state;
    state.debug_set_line(0);
    state.debug_set_field(4);  // empty
    int f4_before = vf.field_counts[3];
    State::InputState in;
    in.a_pressed = true; state.update(vf, in);
    int f4_after = vf.field_counts[3];
    if (f4_before != f4_after) {
        printf("    FAIL: A changed counts (%d → %d)\n", f4_before, f4_after);
        return 1;
    }
    in = State::InputState{};
    in.b_pressed = true; state.update(vf, in);
    if (f4_after != vf.field_counts[3]) {
        printf("    FAIL: B changed counts\n");
        return 1;
    }
    printf("    OK\n");
    return 0;
}

int main()
{
    int rc = 0;
    rc |= test_modes();
    rc |= test_r_held();
    rc |= test_b_keeps_field();
    rc |= test_dpad_box_switch();
    rc |= test_undo_one_shot();
    rc |= test_dpad_clears_undo();
    rc |= test_browse();
    rc |= test_empty_box();
    rc |= test_empty_box_noop();
    if (rc) {
        printf("\nFAIL\n");
        return 1;
    }
    printf("\nPASS: state machine end-to-end (9 scenarios)\n");
    return 0;
}
