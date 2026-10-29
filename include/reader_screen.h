#pragma once
// gbamp3 v1.8 look for every screen except the book page: white background,
// centred 5x7 header, 17-pixel rows of 16-pixel text, light-blue selected row.
// Everything is drawn into the 240x160 8-bit bitmap page through 16-bit
// stores only (VRAM takes no byte writes).
#include <cstdint>

namespace reader::screen {

// Background palette indices (main.cpp palette_colors).
constexpr uint8_t WHITE = 0;
constexpr uint8_t BLACK = 1;
constexpr uint8_t BLUE = 4;  // gbamp3 0x7f55, RGB(170,210,255)
constexpr uint8_t GREY = 5;  // gbamp3 0x4210

constexpr int WIDTH = 240;
constexpr int HEIGHT = 160;
constexpr int ROW_Y = 16;  // first row; the header is above it
constexpr int ROW_H = 17;
constexpr int ROWS = 8;   // on the normal 240x160 screen
constexpr int TEXT_X = 6;

// Size of the picture being drawn: 240x160, or 160x240 for a sideways book
// (drawn into a work buffer and turned onto the screen). The pitch equals
// the width. Everything below draws into the current canvas.
void set_canvas(int width, int height);
// Draw only up to this width (same rows); set_canvas restores the whole width.
void set_width(int w);
int width();
int height();
// List rows that fit below the header: 8 on the normal screen, 13 sideways.
int rows();

void fill(uint8_t* pixels, int x, int y, int w, int h, uint8_t color);
int small_width(const char* text);
void small_draw(uint8_t* pixels, int x, int y, const char* text, uint8_t color);
// Large (16 px) text clipped to [x, x + width): whole, or cut with "...".
// skip > 0 scrolls it left by that many pixels instead (marquee).
void text_fit(uint8_t* pixels, int x, int y, int width, const char* text,
              uint8_t color, unsigned skip = 0);
int text_width(const char* text);
void center_text(uint8_t* pixels, int y, const char* text, uint8_t color = BLACK);
// White header strip with the centred title; right: grey text at the right
// edge (a page number).
void header(uint8_t* pixels, const char* title, const char* right = nullptr);
// Row slot 0..ROWS-1. info: grey small text at the right end of the row.
void row(uint8_t* pixels, int slot, const char* text, bool selected,
         bool folder = false, const char* info = nullptr, unsigned skip = 0);
// Width available to a row's text (marquee limit).
int row_text_width(bool folder, const char* info);
// Grey 5x7 label at the left of a row, with a thin grey line to the right
// edge (the "This book" divider in Settings).
void divider(uint8_t* pixels, int slot, const char* label);
// Splits text into lines of at most width pixels at spaces (a word longer
// than a line is cut). Writes each line's start offset and length; returns
// the number of lines (at most max).
int wrap(const char* text, int width, int* starts, int* lengths, int max);
// gbamp3 message: small black text on a white strip near the bottom.
// A check mark (8 x 7) for the chosen option of a list.
void tick(uint8_t* pixels, int x, int y, uint8_t color);
void message(uint8_t* pixels, const char* text);

}
