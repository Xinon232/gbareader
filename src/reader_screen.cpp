#include "reader_screen.h"
#include "ui_small_glyphs.h"
extern "C" {
#include "font_render.h"
}

namespace reader::screen {
namespace {

int canvas_w = WIDTH;   // drawing stops here
int canvas_h = HEIGHT;
int pitch = WIDTH;      // bytes from one row of pixels to the next

inline void put(uint8_t* pixels, int x, int y, uint8_t color)
{
    if(x < 0 || x >= canvas_w || y < 0 || y >= canvas_h) return;
    volatile uint16_t* pair = reinterpret_cast<volatile uint16_t*>(pixels + y * pitch + (x & ~1));
    *pair = x & 1 ? uint16_t((*pair & 0x00ff) | (color << 8)) : uint16_t((*pair & 0xff00) | color);
}

int clamp_left(int& x, int& w)
{
    if(x < 0) { w += x; x = 0; }
    if(x + w > canvas_w) w = canvas_w - x;
    return w;
}

}

void set_canvas(int w, int h) { canvas_w = w; canvas_h = h; pitch = w; }
void set_width(int w) { canvas_w = w < pitch ? w : pitch; }
int width() { return canvas_w; }
int height() { return canvas_h; }
int rows() { return (canvas_h - ROW_Y) / ROW_H; }

void fill(uint8_t* pixels, int x, int y, int w, int h, uint8_t color)
{
    if(y < 0) { h += y; y = 0; }
    if(y + h > canvas_h) h = canvas_h - y;
    if(clamp_left(x, w) <= 0 || h <= 0) return;
    const uint16_t both = uint16_t(color | (color << 8));
    for(int j = y; j < y + h; ++j) {
        int a = x, n = w;
        if(a & 1) { put(pixels, a, j, color); ++a; --n; }
        volatile uint16_t* line = reinterpret_cast<volatile uint16_t*>(pixels + j * pitch + a);
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
    uint8_t* at = pixels + y * pitch + x;
    if(skip || text_width(text) <= width) {
        draw_text_idx8_bus16_range(text, at, skip, unsigned(width), unsigned(pitch), color);
        return;
    }
    // Cut at a character boundary with room for "...".
    const unsigned dots = font_width("...");
    const unsigned keep = font_width_cap(text, width > int(dots) ? unsigned(width) - dots : 0);
    char cut[260];
    unsigned n = keep < sizeof(cut) - 4 ? keep : sizeof(cut) - 4;
    for(unsigned i = 0; i < n; ++i) cut[i] = text[i];
    cut[n] = '.'; cut[n + 1] = '.'; cut[n + 2] = '.'; cut[n + 3] = 0;
    draw_text_idx8_bus16_range(cut, at, 0, unsigned(width), unsigned(pitch), color);
}

void center_text(uint8_t* pixels, int y, const char* text, uint8_t color)
{
    int w = text_width(text);
    if(w > canvas_w - 2 * TEXT_X) w = canvas_w - 2 * TEXT_X;
    text_fit(pixels, (canvas_w - w) / 2, y, w, text, color);
}

void header(uint8_t* pixels, const char* title, const char* right)
{
    fill(pixels, 0, 0, canvas_w, ROW_Y, WHITE);
    small_draw(pixels, (canvas_w - small_width(title)) / 2, 5, title, BLACK);
    if(right) small_draw(pixels, canvas_w - TEXT_X - small_width(right), 5, right, GREY);
}

int row_text_width(bool folder, const char* info)
{
    int right = canvas_w - (folder ? 18 : 6);
    if(info) right -= small_width(info) + 6;
    return right - TEXT_X;
}

void row(uint8_t* pixels, int slot, const char* text, bool selected, bool folder,
         const char* info, unsigned skip)
{
    const int y = ROW_Y + slot * ROW_H;
    const uint8_t bg = selected ? BLUE : WHITE;
    fill(pixels, 0, y, canvas_w, ROW_H, bg);
    if(!text) return;
    text_fit(pixels, TEXT_X, y + 1, row_text_width(folder, info), text, BLACK, skip);
    if(info) small_draw(pixels, canvas_w - (folder ? 18 : 6) - small_width(info), y + 6, info, GREY);
    if(folder) {
        // gbamp3 icon_folder: tab, body, hollow inside in the row colour.
        const int x = canvas_w - 14;
        fill(pixels, x, y + 4, 4, 2, BLACK);
        fill(pixels, x, y + 6, 10, 7, BLACK);
        fill(pixels, x + 1, y + 7, 8, 5, bg);
    }
}

void divider(uint8_t* pixels, int slot, const char* label)
{
    const int y = ROW_Y + slot * ROW_H;
    fill(pixels, 0, y, canvas_w, ROW_H, WHITE);
    small_draw(pixels, TEXT_X, y + 6, label, GREY);
    const int x = TEXT_X + small_width(label) + 6;
    fill(pixels, x, y + 9, canvas_w - TEXT_X - x, 1, GREY);
}

int wrap(const char* text, int width, int* starts, int* lengths, int max)
{
    int count = 0, at = 0;
    while(text[at] && count < max) {
        while(text[at] == ' ') ++at;
        if(!text[at]) break;
        const char* rest = text + at;
        int take;
        if(text_width(rest) <= width) {
            take = 0;
            while(rest[take]) ++take;
        } else {
            unsigned used = 0;
            take = int(font_width_cap_space(rest, unsigned(width), &used));
            if(take <= 0) take = int(font_width_cap(rest, unsigned(width)));
            if(take <= 0) take = 1;
        }
        int shown = take;
        while(shown > 0 && rest[shown - 1] == ' ') --shown;
        starts[count] = at;
        lengths[count] = shown;
        ++count;
        at += take;
    }
    return count;
}

void tick(uint8_t* pixels, int x, int y, uint8_t color)
{
    // A check mark, 8 x 7.
    static constexpr uint8_t rows[7] = {0x01, 0x03, 0x86, 0xCC, 0x78, 0x30, 0x00};
    for(int j = 0; j < 7; ++j)
        for(int i = 0; i < 8; ++i)
            if(rows[j] & (0x80 >> i)) put(pixels, x + i, y + j, color);
}

void message(uint8_t* pixels, const char* text)
{
    fill(pixels, 0, canvas_h - 20, canvas_w, 16, WHITE);
    small_draw(pixels, (canvas_w - small_width(text)) / 2, canvas_h - 15, text, BLACK);
}

}
