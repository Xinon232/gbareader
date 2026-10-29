#pragma once
// Per-book settings ("This book" in Settings), kept in the spare bytes of each
// .sav state slot. The slot checksum already covers them; earlier versions
// ignore the bytes and keep them zero, which reads back as the defaults.
#include "reader_core.h"
#include <cstdint>

namespace reader {

constexpr int COMIC_ZOOMS = 16;

struct BookPrefs {
    Angle angle = Angle::NORMAL;
    bool comic = false;   // a .gbcomic
    // Comics: the zoom (its number in the file's list) upright and sideways,
    // reading order, screens with nothing but paper passed over.
    uint8_t zoom = 0;
    uint8_t side_zoom = 0;
    uint8_t order = 0;
    bool skip_empty = true;
    // Screens across (high 4 bits) and down (low), 0: as many as needed. One for
    // the book: every zoom, both angles.
    uint8_t counts = 0;
    // Read from a comic record (not stored, not compared): a book seen before.
    bool comic_known = false;
};

// Two records: the book's (16 bytes, "BKP1") and a comic's (32, "BKC2").
constexpr uint32_t BOOK_PREFS_AT = 864;  // Inside the 1024-byte slot, after the 800-byte state.
constexpr uint32_t BOOK_PREFS_BYTES = 48;

inline bool same_book_prefs(const BookPrefs& a, const BookPrefs& b)
{
    if(a.angle != b.angle || a.comic != b.comic || a.zoom != b.zoom || a.side_zoom != b.side_zoom ||
       a.order != b.order || a.skip_empty != b.skip_empty)
        return false;
    if(a.counts != b.counts) return false;
    return true;
}

inline BookPrefs default_book_prefs(bool comic_file)
{
    BookPrefs prefs;
    prefs.comic = comic_file;
    return prefs;
}

inline void encode_book_prefs(const BookPrefs& prefs, unsigned char (&out)[BOOK_PREFS_BYTES])
{
    for(unsigned char& b : out) b = 0;
    out[0] = 'B'; out[1] = 'K'; out[2] = 'P'; out[3] = '1';
    out[4] = uint8_t(prefs.angle);
    out[5] = prefs.comic ? 1 : 0;
    out[6] = prefs.zoom;
    out[7] = prefs.order;
    out[8] = 1;   // (V3.3 test builds: margins light)
    // Zero bytes (records written before these settings) read as the defaults.
    out[10] = uint8_t(prefs.skip_empty ? 0 : 2);
    out[14] = 255; out[15] = 255;
    unsigned char* c = out + 16;
    c[0] = 'B'; c[1] = 'K'; c[2] = 'C'; c[3] = '2';
    // c[4..7]: zero (test builds kept margins there).
    // c[8]: zero. V3.3 test builds kept the page map (bit 0) and the
    // animations (bits 1-6) here; both are global settings now.
    c[9] = prefs.side_zoom;
    c[12] = prefs.counts;   // c[13..27]: zero (test builds: one per zoom)
}

// False (and prefs untouched) when the bytes hold no valid record. A missing
// or damaged comic record leaves the comic settings as they were.
inline bool decode_book_prefs(const unsigned char* in, BookPrefs& prefs)
{
    if(in[0] != 'B' || in[1] != 'K' || in[2] != 'P' || in[3] != '1') return false;
    if(in[4] >= ANGLE_COUNT || in[5] > 1) return false;
    prefs.angle = Angle(in[4]);
    prefs.comic = in[5] == 1;
    prefs.zoom = in[6];
    prefs.order = in[7];
    prefs.skip_empty = !(in[10] & 2);
    const unsigned char* c = in + 16;
    if(c[0] != 'B' || c[1] != 'K' || c[2] != 'C' || c[3] != '2') return true;   // (BKC1: V3.3 test builds)
    const int pad = (c[8] >> 1) & 3, ab = (c[8] >> 3) & 3, row = (c[8] >> 5) & 3;
    if(c[8] > 127 || pad >= SLIDE_COUNT || ab >= SLIDE_COUNT || row >= SLIDE_COUNT) return true;
    prefs.side_zoom = c[9];
    prefs.counts = c[12];
    prefs.comic_known = true;
    return true;
}

}
