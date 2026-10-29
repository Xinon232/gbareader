#ifndef READER_HOLD_H
#define READER_HOLD_H


namespace reader {
// gbawriter v1.2.0 (577ff2a): NAV_REPEAT_DELAY=24,
// CAPS_HOLD_DELAY=2*NAV_REPEAT_DELAY. Its initial press is frame zero.
constexpr unsigned READER_HOLD_FRAMES = 48;
struct ReaderHold {
    // Hold Up alone toggles L/R page turns; Down has no hold action.
    unsigned previous : 2;
    unsigned pending : 2;
    unsigned frames : 6;
    unsigned mode_message_frames : 6;
    bool shoulder_page_turns : 1;
    ReaderHold() : previous(0), pending(0), frames(0), mode_message_frames(0),
                   shoulder_page_turns(false) {}
    unsigned update(unsigned held, bool reading) {
        const unsigned pressed = held & ~previous;
        previous = held & 3; // Only Up/Down edges can arm this gate.
        if(!reading || held != pending) { pending = 0; frames = 0; }
        if(!reading) return 0;
        if(held == 1 && (pressed & held)) {
            pending = held;
            frames = 0;
        } else if(pending && ++frames >= READER_HOLD_FRAMES) {
            pending = 0;
            return held;
        }
        return 0;
    }
};
static_assert(sizeof(ReaderHold) <= 4);
}
#endif
