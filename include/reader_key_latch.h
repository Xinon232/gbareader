#pragma once
// Every press counts: the keys are read at each VBlank (an interrupt, also
// while the main loop is busy reading the card or laying out a page) and
// each change is kept in order. The main loop takes them one press at a
// time, so presses shorter than a busy stretch, or several in one, are all
// seen, and in the order they came.
#include <cstdint>

namespace rkey {

// Keys as KEYINPUT has them (1 = held).
constexpr unsigned A = 1, B = 2, SELECT = 4, START = 8, RIGHT = 16, LEFT = 32, UP = 64, DOWN = 128,
                   R = 256, L = 512, ALL = 1023;

// What the main loop sees in a frame.
struct Frame {
    unsigned held = 0, pressed = 0;
};

struct KeyLatch {
    // Changes waiting (a press and its release are two): 150 quick page
    // turns of a long book, 3 frames apart, while each turn takes 6.
    static constexpr unsigned SIZE = 512;
    volatile uint16_t changes[SIZE];
    // (No initial values: it lives in EWRAM .sbss; start() sets it up.)
    volatile uint16_t head;   // written by the interrupt
    volatile uint16_t tail;   // written by the main loop
    uint16_t sampled;         // the interrupt's last reading
    uint16_t seen;            // the keys as the main loop saw them last

    // The keys held now (at start: not counted as presses).
    void start(unsigned keys)
    {
        head = 0;
        tail = 0;
        sampled = seen = uint16_t(keys);
    }

    // At each VBlank: the keys held now.
    void sample(unsigned keys)
    {
        if(keys == sampled) return;
        sampled = uint16_t(keys);
        const unsigned at = head, next = (at + 1) % SIZE;
        // Full: the newest change is replaced (the main loop never reads that one then).
        if(next == tail) changes[(at + SIZE - 1) % SIZE] = uint16_t(keys);
        else { changes[at] = uint16_t(keys); head = uint16_t(next); }
    }

    // Once a main loop: the changes up to the next press. Releases go with
    // it, except that Start let go has a loop of its own before the next
    // press (Start saves when it is let go).
    Frame next()
    {
        unsigned released = 0;
        Frame f;
        while(tail != head) {
            const unsigned keys = changes[tail];
            const unsigned pressed = keys & ~unsigned(seen);
            if(pressed && (released & START)) break;
            released |= seen & ~keys;
            seen = uint16_t(keys);
            tail = uint16_t((tail + 1) % SIZE);
            if(pressed) { f.pressed = pressed; break; }
        }
        f.held = seen;
        return f;
    }

    // Every change waiting, all at once (nothing is handed over): true if
    // there was one. The keys held are then those of the last change.
    bool drain()
    {
        const bool any = tail != head;
        while(tail != head) {
            seen = changes[tail];
            tail = uint16_t((tail + 1) % SIZE);
        }
        return any;
    }
};

// L and R held together, the moment the second (or both) went down. Keys
// held at start are no press, so L+R held at boot (SuperCard driver) is no
// chord until one of them is let go and pressed again.
inline bool lr_chord(const Frame& f)
{
    return (f.held & (L | R)) == (L | R) && (f.pressed & (L | R));
}

// After waking from sleep: every key is ignored until all of them have been
// up for QUIET frames in a row, so the START held 1 s that woke the GBA (or a
// bouncing START contact) does nothing, and keys held meanwhile do nothing
// when they are let go.
struct WakeGuard {
    static constexpr unsigned QUIET = 6;
    bool active = false;
    unsigned quiet = 0;

    void start() { active = true; quiet = 0; }

    // Once a main loop while active: the keys held now and whether any key
    // changed since the last loop (then only this loop counts as up).
    void frame(unsigned held, bool changed)
    {
        quiet = held ? 0 : changed ? 1 : quiet + 1;
        if(quiet >= QUIET) active = false;
    }
};

}
