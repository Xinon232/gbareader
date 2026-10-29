#pragma once
// The keypad as the reader sees it: while a turned book (or its Settings) is
// on screen, the D-pad turns with the picture. A, B, L, R, Start and Select
// keep their place. The keys of the frame come from the VBlank latch
// (reader_key_latch.h: every press counts, also while the loop is busy);
// tests set frame_ref() themselves.
#include "reader_key_latch.h"
#include "reader_turn.h"

namespace rkey {

inline reader::Angle& angle_ref() { static reader::Angle angle = reader::Angle::NORMAL; return angle; }
inline void set_angle(reader::Angle angle) { angle_ref() = angle; }
inline Frame& frame_ref() { static Frame frame; return frame; }
inline WakeGuard& wake_guard_ref() { static WakeGuard guard; return guard; }

#ifdef __DEVKITARM__
// The latch filled by the VBlank interrupt (src/reader_keys.iwram.cpp).
KeyLatch& latch();
// After bn::core::init(): the keys are read at each VBlank from now on.
void start_latch();
// Once at the start of each main loop: this frame's keys (none while the
// wake-up guard lasts: those changes are dropped).
inline void next_frame()
{
    WakeGuard& guard = wake_guard_ref();
    if(guard.active) {
        const bool changed = latch().drain();
        guard.frame(latch().seen, changed);
        frame_ref() = Frame{};
        return;
    }
    frame_ref() = latch().next();
}
// The book list's L+R: sleep (screen off, Stop mode) until START is held for
// 1 s (let go sooner: Stop again, the screen still off), then the keys are
// ignored until all have been up for a moment (main.cpp).
void sleep_until_start();
#endif

inline bool lr_chord() { return rkey::lr_chord(frame_ref()); }

inline unsigned physical_held()
{
    const unsigned k = frame_ref().held;
    return unsigned(bool(k & UP)) | (unsigned(bool(k & DOWN)) << 1) |
           (unsigned(bool(k & LEFT)) << 2) | (unsigned(bool(k & RIGHT)) << 3);
}
inline unsigned physical_pressed()
{
    const unsigned k = frame_ref().pressed;
    return unsigned(bool(k & UP)) | (unsigned(bool(k & DOWN)) << 1) |
           (unsigned(bool(k & LEFT)) << 2) | (unsigned(bool(k & RIGHT)) << 3);
}
inline bool seen_held(unsigned key) { return reader::turn_keys(physical_held(), angle_ref()) & key; }
inline bool seen_pressed(unsigned key) { return reader::turn_keys(physical_pressed(), angle_ref()) & key; }

inline bool up_held() { return seen_held(reader::KEY_UP); }
inline bool down_held() { return seen_held(reader::KEY_DOWN); }
inline bool left_held() { return seen_held(reader::KEY_LEFT); }
inline bool right_held() { return seen_held(reader::KEY_RIGHT); }
inline bool up_pressed() { return seen_pressed(reader::KEY_UP); }
inline bool down_pressed() { return seen_pressed(reader::KEY_DOWN); }
inline bool left_pressed() { return seen_pressed(reader::KEY_LEFT); }
inline bool right_pressed() { return seen_pressed(reader::KEY_RIGHT); }

inline bool a_held() { return frame_ref().held & A; }
inline bool b_held() { return frame_ref().held & B; }
inline bool l_held() { return frame_ref().held & L; }
inline bool r_held() { return frame_ref().held & R; }
inline bool start_held() { return frame_ref().held & START; }
inline bool select_held() { return frame_ref().held & SELECT; }
inline bool a_pressed() { return frame_ref().pressed & A; }
inline bool b_pressed() { return frame_ref().pressed & B; }
inline bool l_pressed() { return frame_ref().pressed & L; }
inline bool r_pressed() { return frame_ref().pressed & R; }
inline bool start_pressed() { return frame_ref().pressed & START; }
inline bool select_pressed() { return frame_ref().pressed & SELECT; }
inline bool any_pressed() { return frame_ref().pressed; }

}
