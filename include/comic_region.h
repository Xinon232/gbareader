#pragma once
// The decoded part of the comic picture on view, kept around the screen so
// moving it only unpacks what comes in. The memory holds a few strips of the
// picture (as stored: half a screen wide, 16 rows a chunk) wrapped round:
// picture pixel (x, y) is at (x mod width, y mod height), so a moving screen
// reuses what it had and loads one chunk at a time ahead of it.
#include "gbcomic.h"
#include "reader_turn.h"

#include <cstddef>
#include <cstdint>

namespace reader::gbcomic {

class Region {
public:
    void set_memory(uint8_t* memory, size_t bytes) { _memory = memory; _bytes = bytes; _picture = nullptr; }
    // The picture to show; what was loaded is forgotten unless it is the same one.
    bool start(const Picture& picture, int screen_w, int screen_h);
    void forget() { _picture = nullptr; _at = 0; }
    bool started() const { return _picture != nullptr; }
    // Loads all of the screen at (x, y). False: the book could not be read.
    bool load(Book& book, int x, int y) { int unused; return load_rect(book, x, y, _screen_w, _screen_h, 1 << 30, ~0u, unused); }
    // Loads a little more around the screen at (x, y) going (dx, dy) (signs):
    // at most budget chunks and max_bytes packed bytes (read time). True when
    // there is nothing more to load there, or no time for it.
    bool load_ahead(Book& book, int x, int y, int dx, int dy, int budget, uint32_t max_bytes);
    // While waiting at the screen at (x, y): loads the way to the screen at
    // (rx, ry) and that screen, as much as fits beside the screen shown
    // (nearest it first); true when done (or no time).
    bool load_beside(Book& book, int x, int y, int rx, int ry, int budget, uint32_t max_bytes);
    // Packed bytes of the chunks of rectangle (x, y, w, h) not in memory now
    // (those whose index is not at hand count as nothing).
    uint32_t packed_missing(const Book& book, int x, int y, int w, int h) const;
    // Draws canvas rectangle (cx, cy, w, h) of the screen at (x, y) (loaded).
    void blit(int x, int y, int cx, int cy, int w, int h, uint16_t* screen_page, Angle angle) const;
    int strips_kept() const { return _strips; }
    int chunks_kept() const { return _chunks; }

private:
    // loaded: chunks; -1 when the next chunk needs more than max_bytes.
    bool load_rect(Book& book, int x, int y, int w, int h, int budget, uint32_t max_bytes, int& loaded);
    void cover(int x, int y, int w, int h, int& s0, int& s1, int& k0, int& k1) const;

    static constexpr int MAX_SLOTS = 8, MAX_ROWS = 40;
    uint8_t* _memory = nullptr;
    size_t _bytes = 0;
    const Picture* _picture = nullptr;
    uint32_t _at = 0;
    int _screen_w = 240, _screen_h = 160;
    int _strips = 0, _chunks = 0;    // kept: strips across, chunk rows down
    int _width = 0, _height = 0;     // in pixels
    int16_t _strip_of[MAX_SLOTS];    // the strip in each slot column, -1 none
    int16_t _chunk_of[MAX_SLOTS][MAX_ROWS];
};

}
