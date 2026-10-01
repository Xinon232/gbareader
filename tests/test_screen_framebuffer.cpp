// The gbamp3-style screens drawn by the real reader_screen code with the
// shipped SuperFW fonts into a host copy of the 240x160 8-bit page.
#include "reader_screen.h"
#include "ui_small_glyphs.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
extern "C" {
#include "font_render.h"
void* font_base_addr;
void* reader_font_base_addr;
}
namespace screen = reader::screen;
using Pixels = std::array<uint8_t, 240 * 160>;

static std::vector<unsigned char> load(const char* path)
{
    FILE* f = std::fopen(path, "rb"); assert(f);
    std::fseek(f, 0, SEEK_END); long size = std::ftell(f); assert(size > 0); std::rewind(f);
    std::vector<unsigned char> bytes(size);
    assert(std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size()); std::fclose(f);
    return bytes;
}
static int count(const Pixels& p, int x, int y, int w, int h, uint8_t c)
{
    int n = 0;
    for(int j = y; j < y + h; ++j) for(int i = x; i < x + w; ++i) n += p[j * 240 + i] == c;
    return n;
}
static Pixels text_only(const char* s, int x, int y, int w)
{
    Pixels p{};
    draw_text_idx8_bus16_range(s, p.data() + y * 240 + x, 0, w, 240, screen::BLACK);
    return p;
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    auto base = load(argv[1]), symbols = load(argv[2]);
    font_base_addr = base.data(); reader_font_base_addr = symbols.data();

    // Header: white strip, 5x7 title centred at y 5 (gbamp3), grey page number at the right.
    Pixels p; p.fill(9);
    screen::header(p.data(), "Settings", "2/7");
    assert(count(p, 0, 0, 240, 16, 9) == 0);
    const int title_x = (240 - screen::small_width("Settings")) / 2;
    assert(screen::small_width("Settings") == 8 * 6 - 1);
    for(int j = 0; j < 7; ++j)
        for(int k = 0; k < 5; ++k)
            assert((p[(5 + j) * 240 + title_x + k] == screen::BLACK) == bool(small_bits('S', j) & (16 >> k)));
    assert(count(p, 0, 0, 120 + 40, 16, screen::GREY) == 0 && count(p, 200, 0, 40, 16, screen::GREY) > 0);
    assert(count(p, 0, 16, 240, 144, 9) == 240 * 144); // Nothing below the header.

    // Rows: 17 px slots from y 16; the selected one is a full-width light-blue bar.
    p.fill(screen::WHITE);
    screen::row(p.data(), 0, "Alice.txt", true);
    screen::row(p.data(), 1, "Moby Dick.epub", false);
    assert(count(p, 0, 16, 240, 17, screen::BLUE) + count(p, 0, 16, 240, 17, screen::BLACK) == 240 * 17);
    assert(count(p, 0, 33, 240, 17, screen::BLUE) == 0);
    // Text pixels exactly as the SuperFW renderer draws them at x 6, one line below the row top.
    Pixels expect = text_only("Moby Dick.epub", 6, 34, 228);
    for(int i = 33 * 240; i < 50 * 240; ++i) assert((p[i] == screen::BLACK) == (expect[i] == screen::BLACK));
    // All eight rows fit above the bottom edge.
    assert(screen::ROW_Y + screen::ROWS * screen::ROW_H <= 160);

    // A long name is cut with "..." inside the row; a scrolled one (marquee) is not.
    const std::string longest(200, 'W');
    p.fill(screen::WHITE);
    screen::row(p.data(), 2, longest.c_str(), false);
    const int y = 16 + 2 * 17;
    assert(count(p, 234, y, 6, 17, screen::BLACK) == 0);
    const unsigned dots = font_width("...");
    assert(count(p, 234 - int(dots) - 2, y, int(dots) + 2, 17, screen::BLACK) > 0);
    std::string cut = longest.substr(0, font_width_cap(longest.c_str(), 228 - dots)) + "...";
    expect = text_only(cut.c_str(), 6, y + 1, 228);
    for(int i = y * 240; i < (y + 17) * 240; ++i) assert((p[i] == screen::BLACK) == (expect[i] == screen::BLACK));
    p.fill(screen::WHITE);
    screen::row(p.data(), 2, "abcdefghijklmnopqrstuvwxyz abcdefghijklmnopqrstuvwxyz", true, false, nullptr, 7);
    Pixels scrolled{};
    draw_text_idx8_bus16_range("abcdefghijklmnopqrstuvwxyz abcdefghijklmnopqrstuvwxyz",
                               scrolled.data() + (y + 1) * 240 + 6, 7, 228, 240, screen::BLACK);
    for(int i = y * 240; i < (y + 17) * 240; ++i)
        assert((p[i] == screen::BLACK) == (scrolled[i] == screen::BLACK));

    // Folder rows: gbamp3 folder icon at x 226, hollow in the row colour; text stops at 222.
    p.fill(screen::WHITE);
    screen::row(p.data(), 0, "Downloads", true, true);
    assert(count(p, 226, 20, 10, 9, screen::BLACK) == 4 * 2 + 10 * 7 - 8 * 5);
    assert(count(p, 227, 23, 8, 5, screen::BLUE) == 40);
    assert(screen::row_text_width(true, nullptr) == 216 && screen::row_text_width(false, nullptr) == 228);
    // Settings info: grey small text at the right end, the label narrower to leave room.
    p.fill(screen::WHITE);
    screen::row(p.data(), 1, "Line spacing: 1", false, false, "9 lines");
    assert(count(p, 234 - screen::small_width("9 lines"), 33, screen::small_width("9 lines"), 17, screen::GREY) > 0);
    assert(screen::row_text_width(false, "9 lines") == 228 - screen::small_width("9 lines") - 6);

    // Centred text and the message strip near the bottom.
    p.fill(screen::BLUE);
    screen::message(p.data(), "Imported");
    assert(count(p, 0, 140, 240, 16, screen::BLUE) == 0 && count(p, 0, 140, 240, 16, screen::BLACK) > 0);
    p.fill(screen::WHITE);
    screen::center_text(p.data(), 64, "No books found");
    const int w = screen::text_width("No books found");
    expect = text_only("No books found", (240 - w) / 2, 64, w);
    for(size_t i = 0; i < p.size(); ++i) assert((p[i] == screen::BLACK) == (expect[i] == screen::BLACK));
    // Odd-x fills and clipping never touch neighbours or leave the page.
    p.fill(screen::WHITE);
    screen::fill(p.data(), 3, 3, 5, 2, screen::GREY);
    assert(count(p, 0, 0, 240, 160, screen::GREY) == 10 && p[3 * 240 + 2] == screen::WHITE && p[3 * 240 + 8] == screen::WHITE);
    screen::fill(p.data(), -10, 150, 300, 40, screen::BLACK);
    assert(count(p, 0, 150, 240, 10, screen::BLACK) == 2400);
    std::puts("PASS: gbamp3 screens: header, rows, light-blue bar, ellipsis, marquee, folder icon, info, message, fills");
}
