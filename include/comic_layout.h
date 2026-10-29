#pragma once
// Where the screens of a comic picture go: as many as cover the picture
// (or as many as the book says), spread evenly, the same on every page.
// Plain functions, so the host tests check them.
#include "gbcomic.h"

#include <cstdint>

namespace reader::comic_layout {

constexpr int MAX_ALONG = 24;   // screens across or down one picture
constexpr int MAX_COUNT = 9;    // the most a book can ask for

struct Grid {
    int pic_w = 1, pic_h = 1, screen_w = 240, screen_h = 160;
    int columns = 1, rows = 1;
    int16_t xs[MAX_ALONG] = {}, ys[MAX_ALONG] = {};  // each column's left, each row's top (negative: centred)
};

// Screens for span pixels, size each: one if it fits, an exact number when
// the screens fit exactly, else as many as leave at least 16 pixels of
// overlap.
int auto_count(int span, int size);
// columns, rows: 0 for as many as needed. A count too small for the picture
// shares what the screens leave over evenly before, between and after them
// (that part is passed over).
void make_grid(int pic_w, int pic_h, int screen_w, int screen_h, int columns, int rows, Grid& grid);
// How far a screen can go (a picture smaller than the screen stays centred).
void limits(const Grid& grid, int& min_x, int& max_x, int& min_y, int& max_y);
// Screens in reading order over the pictures of a view.
struct Spot {
    int picture = 0, column = 0, row = 0;
};
int spot_count(const Grid* grids, int pictures);
Spot spot_at(const Grid* grids, int pictures, gbcomic::Order order, int index);
int spot_index(const Grid* grids, int pictures, gbcomic::Order order, const Spot& spot);
// Same line: the same row (the same column for Columns) of the same picture.
bool same_line(const Spot& a, const Spot& b, gbcomic::Order order);

// One bit per CELL x CELL pixels of a picture: where its ink is.
struct Cells {
    uint8_t* bits = nullptr;
    int w = 0, h = 0;          // in cells; 0: unknown
    bool known() const { return bits && w > 0 && h > 0; }
};
// Any ink under the screen at (x, y) (true when not known).
bool any_ink(const Cells& ink, int x, int y, int w, int h);

}
