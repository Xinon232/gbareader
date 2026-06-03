// render_mock.cpp — host-side mock of the GBA screen.
// Compiles with: g++ -std=c++17 render_mock.cpp -o render_mock
// (no butano — just stubs out the same API).
//
// Produces 240x160 PNG files showing what the GBA would render.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

#include "vocab.h"

// Mock of butano's APIs
namespace bn {
struct color { uint8_t r, g, b; color(uint8_t r_=0, uint8_t g_=0, uint8_t b_=0) : r(r_), g(g_), b(b_) {} };
}

struct Screen {
    static constexpr int W = 240;
    static constexpr int H = 160;
    bn::color bg{31, 31, 31};  // white
    bn::color text_fg{0, 0, 0};
    int flash_timer = 0;
    bn::color flash_color{0, 0, 0};

    void flash_green() { flash_timer = 10; flash_color = {0, 31, 0}; }
    void flash_red()   { flash_timer = 10; flash_color = {31, 0, 0}; }

    void update_flash() {
        if (flash_timer > 0) {
            flash_timer--;
            if (flash_timer > 5) {
                bg = flash_color;
            } else {
                bg = {31, 31, 31};
            }
        } else {
            bg = {31, 31, 31};
        }
    }
};

// 5x7 bitmap font, 95 chars (ASCII 0x20-0x7E)
static const uint8_t font5x7[95][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, // !
    {0x00,0x00,0x00,0x00,0x11,0x11,0x00}, // "
    {0x00,0x00,0x11,0x3E,0x11,0x3E,0x11}, // #
    {0x1C,0x22,0x22,0x1C,0x22,0x22,0x1C}, // $
    {0x00,0x00,0x24,0x18,0x06,0x12,0x00}, // %
    {0x1C,0x22,0x22,0x1C,0x36,0x22,0x1F}, // &
    {0x00,0x00,0x00,0x00,0x04,0x04,0x08}, // '
    {0x04,0x08,0x10,0x10,0x10,0x08,0x04}, // (
    {0x04,0x02,0x01,0x01,0x01,0x02,0x04}, // )
    {0x04,0x0A,0x11,0x1F,0x11,0x11,0x11}, // *
    {0x00,0x00,0x11,0x1F,0x11,0x1F,0x11}, // +
    {0x00,0x00,0x00,0x00,0x04,0x04,0x08}, // ,
    {0x00,0x00,0x00,0x00,0x11,0x11,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x00,0x10}, // .
    {0x00,0x00,0x00,0x00,0x10,0x10,0x10}, // /
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // 2
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
    {0x00,0x04,0x04,0x00,0x04,0x04,0x00}, // :
    {0x00,0x04,0x04,0x00,0x04,0x04,0x08}, // ;
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, // <
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, // =
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, // >
    {0x0E,0x11,0x02,0x04,0x04,0x00,0x04}, // ?
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, // @
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // A
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // B
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // C
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, // D
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // E
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // F
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}, // G
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // H
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // I
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // J
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // K
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // L
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // M
    {0x11,0x11,0x19,0x15,0x13,0x11,0x11}, // N
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // O
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // P
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // Q
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // R
    {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}, // S
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // T
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // U
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, // V
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, // W
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // X
    {0x11,0x11,0x11,0x0A,0x04,0x04,0x04}, // Y
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // Z
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, // [
    {0x00,0x10,0x10,0x10,0x00,0x10,0x10}, // backslash
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, // ]
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, // _
};

void draw_char(uint8_t* pixels, int x, int y, char c, bn::color color) {
    if (c < 0x20 || c > 0x7E) return;
    int idx = c - 0x20;
    // Convert 5-bit color (0-31) to 8-bit (0-255).
    auto expand = [](uint8_t v) -> uint8_t {
        return (v << 3) | (v >> 2);
    };
    uint8_t r = expand(color.r);
    uint8_t g = expand(color.g);
    uint8_t b = expand(color.b);
    for (int row = 0; row < 7; row++) {
        uint8_t bits = font5x7[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < Screen::W && py >= 0 && py < Screen::H) {
                    int off = (py * Screen::W + px) * 3;
                    pixels[off+0] = r;
                    pixels[off+1] = g;
                    pixels[off+2] = b;
                }
            }
        }
    }
}

void draw_string(uint8_t* pixels, int x, int y, const char* s, bn::color color, int scale = 1) {
    auto expand = [](uint8_t v) -> uint8_t {
        return (v << 3) | (v >> 2);
    };
    uint8_t r = expand(color.r);
    uint8_t g = expand(color.g);
    uint8_t b = expand(color.b);
    for (int i = 0; s[i]; i++) {
        for (int sy = 0; sy < scale; sy++) {
            for (int sx = 0; sx < scale; sx++) {
                char c = s[i];
                int idx = c - 0x20;
                if (c < 0x20 || c > 0x7E) continue;
                for (int row = 0; row < 7; row++) {
                    uint8_t bits = font5x7[idx][row];
                    for (int col = 0; col < 5; col++) {
                        if (bits & (1 << (4 - col))) {
                            int px = x + i * 6 * scale + col * scale + sx;
                            int py = y + row * scale + sy;
                            if (px >= 0 && px < Screen::W && py >= 0 && py < Screen::H) {
                                int off = (py * Screen::W + px) * 3;
                                pixels[off+0] = r;
                                pixels[off+1] = g;
                                pixels[off+2] = b;
                            }
                        }
                    }
                }
            }
        }
    }
}

void draw_string_centered(uint8_t* pixels, int cx, int y, const char* s, bn::color color, int scale = 1) {
    int w = strlen(s) * 6 * scale;
    draw_string(pixels, cx - w / 2, y, s, color, scale);
}

void fill_background(uint8_t* pixels, bn::color color) {
    auto expand = [](uint8_t v) -> uint8_t { return (v << 3) | (v >> 2); };
    uint8_t r = expand(color.r);
    uint8_t g = expand(color.g);
    uint8_t b = expand(color.b);
    for (int i = 0; i < Screen::W * Screen::H; i++) {
        pixels[i*3+0] = r;
        pixels[i*3+1] = g;
        pixels[i*3+2] = b;
    }
}

// Write a PPM file (raw RGB) which can be read by chafa.
void write_ppm(const char* path, const uint8_t* pixels) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", Screen::W, Screen::H);
    fwrite(pixels, 1, Screen::W * Screen::H * 3, f);
    fclose(f);
}

// Write a BMP file (3 bytes per pixel, BGR).
void write_bmp(const char* path, const uint8_t* pixels) {
    int w = Screen::W, h = Screen::H;
    int row_size = (w * 3 + 3) & ~3;
    int data_size = row_size * h;
    int file_size = 54 + data_size;

    uint8_t header[54] = {0};
    header[0] = 'B'; header[1] = 'M';
    *(int*)(header + 2) = file_size;
    *(int*)(header + 10) = 54;
    *(int*)(header + 14) = 40;
    *(int*)(header + 18) = w;
    *(int*)(header + 22) = h;
    *(short*)(header + 26) = 1;
    *(short*)(header + 28) = 24;
    *(int*)(header + 34) = data_size;

    FILE* f = fopen(path, "wb");
    fwrite(header, 1, 54, f);
    uint8_t* row = (uint8_t*)malloc(row_size);
    for (int y = h - 1; y >= 0; y--) {
        const uint8_t* src = pixels + y * w * 3;
        for (int x = 0; x < w; x++) {
            row[x*3+0] = src[x*3+2];  // B
            row[x*3+1] = src[x*3+1];  // G
            row[x*3+2] = src[x*3+0];  // R
        }
        for (int x = w * 3; x < row_size; x++) row[x] = 0;
        fwrite(row, 1, row_size, f);
    }
    free(row);
    fclose(f);
}

int main(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : "data/sample.txt";
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    int sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc(sz);
    fread(data, 1, sz, f);
    fclose(f);
    printf("Loaded %s (%d bytes)\n", path, sz);

    VocabFile vf;
    int n = vocab_open(vf, data, sz);
    printf("Loaded %d vocab pairs\n", n);

    // Render frame for first word.
    LineBuf current;
    vocab_show(vf, data, sz, 0, current);

    uint8_t* pixels = (uint8_t*)calloc(Screen::W * Screen::H * 3, 1);

    Screen screen;
    // 1. White background (Step 4 default).
    screen.bg = {31, 31, 31};
    fill_background(pixels, screen.bg);

    // 2. Header: "Field 1/5 - 472 words"
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "Field 1/5 - %d words", n);
    draw_string_centered(pixels, Screen::W/2, 16, hdr, {0, 0, 0}, 1);

    // 3. Prompt: the current word's a-side
    draw_string_centered(pixels, Screen::W/2, 50, current.a, {0, 0, 0}, 2);

    // 4. Answer (always shown for mock; in real app only on R)
    draw_string_centered(pixels, Screen::W/2, 80, current.b, {0, 0, 0}, 1);

    // 5. Footer: 5 field counts
    char ftr[64];
    snprintf(ftr, sizeof(ftr), "F1:%u F2:%u F3:%u F4:%u F5:%u",
             vf.field_counts[0], vf.field_counts[1], vf.field_counts[2],
             vf.field_counts[3], vf.field_counts[4]);
    draw_string_centered(pixels, Screen::W/2, 120, ftr, {0, 0, 0}, 1);

    write_ppm("/tmp/render_frame.ppm", pixels);
    printf("Wrote /tmp/render_frame.ppm\n");

    // Now simulate A press → green flash
    screen.flash_green();
    screen.update_flash();
    fill_background(pixels, screen.bg);
    draw_string_centered(pixels, Screen::W/2, 16, hdr, {0, 0, 0}, 1);
    draw_string_centered(pixels, Screen::W/2, 50, current.a, {0, 0, 0}, 2);
    draw_string_centered(pixels, Screen::W/2, 80, current.b, {0, 0, 0}, 1);
    draw_string_centered(pixels, Screen::W/2, 120, ftr, {0, 0, 0}, 1);
    write_ppm("/tmp/render_green.ppm", pixels);
    printf("Wrote /tmp/render_green.ppm\n");

    // B press → red flash
    screen.flash_red();
    screen.update_flash();
    fill_background(pixels, screen.bg);
    draw_string_centered(pixels, Screen::W/2, 16, hdr, {0, 0, 0}, 1);
    draw_string_centered(pixels, Screen::W/2, 50, current.a, {0, 0, 0}, 2);
    draw_string_centered(pixels, Screen::W/2, 80, current.b, {0, 0, 0}, 1);
    draw_string_centered(pixels, Screen::W/2, 120, ftr, {0, 0, 0}, 1);
    write_ppm("/tmp/render_red.ppm", pixels);
    printf("Wrote /tmp/render_red.ppm\n");

    free(pixels);
    free(data);
    return 0;
}
