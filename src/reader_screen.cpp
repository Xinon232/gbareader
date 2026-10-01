#include "reader_screen.h"
#include "ui_small_glyphs.h"
extern "C" {
#include "font_render.h"
}

namespace reader::screen {
namespace {

inline void put(uint8_t* pixels, int x, int y, uint8_t color)
{
    if(x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    volatile uint16_t* pair = reinterpret_cast<volatile uint16_t*>(pixels + y * WIDTH + (x & ~1));
    *pair = x & 1 ? uint16_t((*pair & 0x00ff) | (color << 8)) : uint16_t((*pair & 0xff00) | color);
}

int clamp_left(int& x, int& w)
{
    if(x < 0) { w += x; x = 0; }
    if(x + w > WIDTH) w = WIDTH - x;
    return w;
}

}

void fill(uint8_t* pixels, int x, int y, int w, int h, uint8_t color)
{
    if(y < 0) { h += y; y = 0; }
    if(y + h > HEIGHT) h = HEIGHT - y;
    if(clamp_left(x, w) <= 0 || h <= 0) return;
    const uint16_t both = uint16_t(color | (color << 8));
    for(int j = y; j < y + h; ++j) {
        int a = x, n = w;
        if(a & 1) { put(pixels, a, j, color); ++a; --n; }
        volatile uint16_t* line = reinterpret_cast<volatile uint16_t*>(pixels + j * WIDTH + a);
        for(int k = 0; k < n / 2; ++k) line[k] = both;
        if(n & 1) put(pixels, a + n - 1, j, color);
    }
}

int small_width(const char* text)
{
    int w = 0;
    for(; *text; ++text) w += SMALL_ADVANCE;
    return w ? w - 1 : 0;
}

void small_draw(uint8_t* pixels, int x, int y, const char* text, uint8_t color)
{
    for(; *text; ++text, x += SMALL_ADVANCE) {
        const unsigned char c = static_cast<unsigned char>(*text);
        if(c < 33 || c > 126) continue;
        for(int j = 0; j < SMALL_HEIGHT + SMALL_DESCENT; ++j) {
            const unsigned bits = small_bits(c, j);
            for(int k = 0; k < 5; ++k)
                if(bits & (16u >> k)) put(pixels, x + k, y + j, color);
        }
    }
}

int text_width(const char* text)
{
    return int(font_width(text));
}

void text_fit(uint8_t* pixels, int x, int y, int width, const char* text, uint8_t color, unsigned skip)
{
    if(width <= 0) return;
    uint8_t* at = pixels + y * WIDTH + x;
    if(skip || text_width(text) <= width) {
        draw_text_idx8_bus16_range(text, at, skip, unsigned(width), WIDTH, color);
        return;
    }
    // Cut at a character boundary with room for "...".
    const unsigned dots = font_width("...");
    const unsigned keep = font_width_cap(text, width > int(dots) ? unsigned(width) - dots : 0);
    char cut[260];
    unsigned n = keep < sizeof(cut) - 4 ? keep : sizeof(cut) - 4;
    for(unsigned i = 0; i < n; ++i) cut[i] = text[i];
    cut[n] = '.'; cut[n + 1] = '.'; cut[n + 2] = '.'; cut[n + 3] = 0;
    draw_text_idx8_bus16_range(cut, at, 0, unsigned(width), WIDTH, color);
}

void center_text(uint8_t* pixels, int y, const char* text, uint8_t color)
{
    int w = text_width(text);
    if(w > WIDTH - 2 * TEXT_X) w = WIDTH - 2 * TEXT_X;
    text_fit(pixels, (WIDTH - w) / 2, y, w, text, color);
}

void header(uint8_t* pixels, const char* title, const char* right)
{
    fill(pixels, 0, 0, WIDTH, ROW_Y, WHITE);
    small_draw(pixels, (WIDTH - small_width(title)) / 2, 5, title, BLACK);
    if(right) small_draw(pixels, WIDTH - TEXT_X - small_width(right), 5, right, GREY);
}

int row_text_width(bool folder, const char* info)
{
    int right = folder ? 222 : 234;
    if(info) right -= small_width(info) + 6;
    return right - TEXT_X;
}

void row(uint8_t* pixels, int slot, const char* text, bool selected, bool folder,
         const char* info, unsigned skip)
{
    const int y = ROW_Y + slot * ROW_H;
    const uint8_t bg = selected ? BLUE : WHITE;
    fill(pixels, 0, y, WIDTH, ROW_H, bg);
    if(!text) return;
    text_fit(pixels, TEXT_X, y + 1, row_text_width(folder, info), text, BLACK, skip);
    if(info) small_draw(pixels, (folder ? 222 : 234) - small_width(info), y + 6, info, GREY);
    if(folder) {
        // gbamp3 icon_folder: tab, body, hollow inside in the row colour.
        fill(pixels, 226, y + 4, 4, 2, BLACK);
        fill(pixels, 226, y + 6, 10, 7, BLACK);
        fill(pixels, 227, y + 7, 8, 5, bg);
    }
}

void status_mark(uint8_t* pixels, int x, int y, bool ok)
{
    const int left = x - 8;
    for(int t = 0; t < 3; ++t) {  // three pixels thick
        if(ok) {
            // Short stroke down to the corner, long stroke up to the right.
            for(int i = 0; i <= 5; ++i) put(pixels, left + 1 + i, y + 8 + i + t, GREEN);
            for(int i = 0; i <= 10; ++i) put(pixels, left + 6 + i, y + 13 - i + t, GREEN);
        } else {
            for(int i = 0; i <= 14; ++i) {
                put(pixels, left + 1 + i, y + 1 + i + t, RED);
                put(pixels, left + 1 + i, y + 15 - i + t, RED);
            }
        }
    }
}

void message(uint8_t* pixels, const char* text)
{
    fill(pixels, 0, 140, WIDTH, 16, WHITE);
    small_draw(pixels, (WIDTH - small_width(text)) / 2, 145, text, BLACK);
}

}
