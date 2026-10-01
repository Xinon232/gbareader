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
constexpr int ROWS = 8;
constexpr int TEXT_X = 6;

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
// gbamp3 message: small black text on a white strip near the bottom.
void message(uint8_t* pixels, const char* text);

}
