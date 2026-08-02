// Regression test for v0.2.5 held-button feedback.
#include "vocab.h"
#include "state.h"

#include <cstdio>
#include <cstring>

static void load_pairs(VocabFile& vf)
{
    vf.reset();
    const char* data = "one\tuno\ntwo\tdos\nthree\ttres\n";
    vocab_open(vf, data, int(strlen(data)));
}

static int test_a_hold()
{
    VocabFile vf;
    load_pairs(vf);
    State state;
    state.debug_set_field(1);
    state.debug_set_line(0);
    uint32_t pressed_offset = vf.line_offsets[0];

    State::InputState in;
    in.a_pressed = true;
    in.a_held = true;
    state.update(vf, in);

    for(int frame = 0; frame < 80; ++frame) {
        in = State::InputState{};
        in.a_held = true;
        state.update(vf, in);
    }

    if(! state.feedback_active() || ! state.show_answer() ||
       vf.line_offsets[state.current_line_idx()] != pressed_offset) {
        std::printf("FAIL: held A did not preserve feedback card + answer\n");
        return 1;
    }

    state.update(vf, State::InputState{});  // release A
    if(state.feedback_active() || state.show_answer() ||
       vf.line_offsets[state.current_line_idx()] == pressed_offset) {
        std::printf("FAIL: releasing A did not finish feedback and advance\n");
        return 1;
    }
    return 0;
}

static int test_b_hold()
{
    VocabFile vf;
    load_pairs(vf);
    State state;
    state.debug_set_field(1);
    state.debug_set_line(0);
    uint32_t pressed_offset = vf.line_offsets[0];

    State::InputState in;
    in.b_pressed = true;
    in.b_held = true;
    state.update(vf, in);

    for(int frame = 0; frame < 80; ++frame) {
        in = State::InputState{};
        in.b_held = true;
        state.update(vf, in);
    }

    if(! state.feedback_active() || ! state.show_answer() ||
       vf.line_offsets[state.current_line_idx()] != pressed_offset) {
        std::printf("FAIL: held B did not preserve feedback card + answer\n");
        return 1;
    }

    state.update(vf, State::InputState{});  // release B
    if(state.feedback_active() || state.show_answer() ||
       vf.line_offsets[state.current_line_idx()] == pressed_offset) {
        std::printf("FAIL: releasing B did not finish feedback and advance\n");
        return 1;
    }
    return 0;
}

int main()
{
    int rc = test_a_hold() | test_b_hold();
    if(rc) return 1;
    std::printf("PASS: held A/B feedback persists until release\n");
    return 0;
}
