#include "comic_layout.h"

namespace reader::comic_layout {
namespace {

int clamp(int v, int low, int high) { return v < low ? low : v > high ? high : v; }

// Screen starts along a picture pic pixels long; screens of size; count 0
// for auto. Enough screens: the first at the start, the last at the end,
// overlapping evenly. Too few: what they leave over is shared out evenly
// before, between and after them.
int spread(int pic, int size, int count, int16_t* out)
{
    if(pic <= size) { out[0] = int16_t(-((size - pic) / 2)); return 1; }
    const int last = pic - size;
    int n = count > 0 ? count : auto_count(pic, size);
    if(n > MAX_ALONG) n = MAX_ALONG;
    const int left_over = pic - n * size;
    for(int i = 0; i < n; ++i) {
        int v;
        if(left_over <= 0) v = (last * i * 2 + (n - 1)) / (2 * (n - 1));
        else v = i * size + (left_over * (i + 1) * 2 + (n + 1)) / (2 * (n + 1));
        out[i] = int16_t(clamp(v, 0, last));
    }
    return n;
}

}

int auto_count(int span, int size)
{
    if(span <= size) return 1;
    if(span % size == 0) return span / size;
    return 1 + (span - size + (size - 16) - 1) / (size - 16);
}

void make_grid(int pic_w, int pic_h, int screen_w, int screen_h, int columns, int rows, Grid& g)
{
    g.pic_w = pic_w > 0 ? pic_w : 1;
    g.pic_h = pic_h > 0 ? pic_h : 1;
    g.screen_w = screen_w;
    g.screen_h = screen_h;
    g.columns = spread(g.pic_w, screen_w, columns, g.xs);
    g.rows = spread(g.pic_h, screen_h, rows, g.ys);
}

void limits(const Grid& g, int& min_x, int& max_x, int& min_y, int& max_y)
{
    if(g.pic_w > g.screen_w) { min_x = 0; max_x = g.pic_w - g.screen_w; }
    else min_x = max_x = -((g.screen_w - g.pic_w) / 2);
    if(g.pic_h > g.screen_h) { min_y = 0; max_y = g.pic_h - g.screen_h; }
    else min_y = max_y = -((g.screen_h - g.pic_h) / 2);
}

int spot_count(const Grid* grids, int pictures)
{
    int n = 0;
    for(int i = 0; i < pictures; ++i) n += grids[i].columns * grids[i].rows;
    return n;
}

Spot spot_at(const Grid* grids, int pictures, gbcomic::Order order, int index)
{
    Spot s;
    for(int i = 0; i < pictures; ++i) {
        const Grid& g = grids[i];
        const int n = g.columns * g.rows;
        if(index >= n && i + 1 < pictures) { index -= n; continue; }
        if(index >= n) index = n - 1;
        if(index < 0) index = 0;
        s.picture = i;
        if(order == gbcomic::Order::COLUMNS) {
            s.column = index / g.rows;
            s.row = index % g.rows;
        } else {
            s.row = index / g.columns;
            s.column = index % g.columns;
            if(order == gbcomic::Order::RIGHT_TO_LEFT) s.column = g.columns - 1 - s.column;
        }
        return s;
    }
    return s;
}

int spot_index(const Grid* grids, int pictures, gbcomic::Order order, const Spot& s)
{
    int index = 0;
    for(int i = 0; i < s.picture && i < pictures; ++i) index += grids[i].columns * grids[i].rows;
    const Grid& g = grids[s.picture];
    if(order == gbcomic::Order::COLUMNS) return index + s.column * g.rows + s.row;
    const int column = order == gbcomic::Order::RIGHT_TO_LEFT ? g.columns - 1 - s.column : s.column;
    return index + s.row * g.columns + column;
}

bool same_line(const Spot& a, const Spot& b, gbcomic::Order order)
{
    if(a.picture != b.picture) return false;
    return order == gbcomic::Order::COLUMNS ? a.column == b.column : a.row == b.row;
}

namespace {

bool cell(const Cells& c, int x, int y)
{
    const int i = y * c.w + x;
    return c.bits[i >> 3] & (1 << (i & 7));
}

// The cells touched by the screen, clipped to the grid.
void cell_range(const Cells& c, int x, int y, int w, int h, int& cx0, int& cy0, int& cx1, int& cy1)
{
    // (Division rounds towards zero: starts left of or above the picture give 0.)
    cx0 = clamp(x / gbcomic::CELL, 0, c.w);
    cy0 = clamp(y / gbcomic::CELL, 0, c.h);
    const int ex = x + w, ey = y + h;
    cx1 = clamp((ex + gbcomic::CELL - 1) / gbcomic::CELL, 0, c.w);
    cy1 = clamp((ey + gbcomic::CELL - 1) / gbcomic::CELL, 0, c.h);
    if(ex < 0) cx1 = 0;
    if(ey < 0) cy1 = 0;
}

}

bool any_ink(const Cells& ink, int x, int y, int w, int h)
{
    if(!ink.known()) return true;
    int cx0, cy0, cx1, cy1;
    cell_range(ink, x, y, w, h, cx0, cy0, cx1, cy1);
    for(int cy = cy0; cy < cy1; ++cy)
        for(int cx = cx0; cx < cx1; ++cx)
            if(cell(ink, cx, cy)) return true;
    return false;
}

}
