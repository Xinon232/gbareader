#pragma once
// A book page or Settings for a turned book is drawn into a canvas as the
// reader sees it (160x240 sideways, 240x160 upside down) and copied onto the
// 240x160 screen turned. The D-pad turns with it.
#include "reader_core.h"
#include <cstdint>

namespace reader {

constexpr int TURN_CANVAS_BYTES = 240 * 160;

// The canvas as the reader sees it.
inline int turned_width(Angle angle) { return sideways(angle) ? 160 : 240; }
inline int turned_height(Angle angle) { return sideways(angle) ? 240 : 160; }

// Screen pixel (x, y) shows canvas pixel (cx, cy).
//   RIGHT: the GBA turned a quarter to the right (clockwise), D-pad on top.
//   LEFT:  turned a quarter to the left, buttons on top.
inline void turned_source(Angle angle, int x, int y, int& cx, int& cy)
{
    switch(angle) {
    case Angle::RIGHT: cx = 159 - y; cy = x; break;
    case Angle::LEFT: cx = y; cy = 239 - x; break;
    case Angle::UPSIDE_DOWN: cx = 239 - x; cy = 159 - y; break;
    default: cx = x; cy = y; break;
    }
}

// Copies the canvas onto one 240x160 8-bit screen page (16-bit stores only).
void turn_blit(const uint8_t* canvas, uint16_t* screen_page, Angle angle);

// Rectangles of the canvas straight on a 240x160 8-bit screen page, turned
// (comics: no canvas, only the parts that change are drawn). (cx, cy, w, h)
// is in canvas pixels; the screen is written with 16-bit stores only.
// src: w x h pixels, stride bytes a row.
void turn_copy(const uint8_t* src, int stride, int cx, int cy, int w, int h, uint16_t* screen_page, Angle angle);
void turn_fill(uint8_t color, int cx, int cy, int w, int h, uint16_t* screen_page, Angle angle);
// The other way: the canvas rectangle from the screen into out.
void turn_read(const uint16_t* screen_page, Angle angle, int cx, int cy, int w, int h, uint8_t* out, int stride);
// to = from with the canvas moved by (-dx, -dy): what was at canvas (x + dx,
// y + dy) is now at (x, y). The part that comes in is left as it was.
void turn_shift(const uint16_t* from, uint16_t* to, Angle angle, int dx, int dy);

// D-pad bits as the reader sees them. Bits: 1 Up, 2 Down, 4 Left, 8 Right;
// all other bits pass unchanged.
constexpr unsigned KEY_UP = 1, KEY_DOWN = 2, KEY_LEFT = 4, KEY_RIGHT = 8;
unsigned turn_keys(unsigned physical, Angle angle);

}
