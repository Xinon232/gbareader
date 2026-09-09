#include "reader_core.h"
#include "reader_credits.h"
#include "reader_controls.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
extern "C" {
#include "font_render.h"
void* font_base_addr;
void* reader_font_base_addr;
}

static std::vector<unsigned char> load(const char* path)
{
    FILE* file = std::fopen(path, "rb");
    assert(file);
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    assert(size > 0);
    std::rewind(file);
    std::vector<unsigned char> bytes(size);
    assert(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    std::fclose(file);
    return bytes;
}
static int width(uint32_t cp)
{
    char s[5]{};
    if(cp < 128) s[0] = char(cp);
    else if(cp < 2048) { s[0] = char(0xc0 | (cp >> 6)); s[1] = char(0x80 | (cp & 63)); }
    else { s[0] = char(0xe0 | (cp >> 12)); s[1] = char(0x80 | ((cp >> 6) & 63)); s[2] = char(0x80 | (cp & 63)); }
    return int(font_width(s));
}
using Pixels = std::array<uint16_t, 240 * 160 / 2>;
static Pixels render(const char* text)
{
    Pixels pixels{};
    draw_text_idx8_bus16_range(text, reinterpret_cast<uint8_t*>(pixels.data()), 0, 224, 240, 1);
    return pixels;
}
static reader::Page layout(const std::string& text)
{
    reader::MemorySource source(reinterpret_cast<const unsigned char*>(text.data()), text.size());
    reader::Page page{};
    assert(reader::layout_page(source, 0, reader::default_settings(), width, page));
    return page;
}
int main(int argc, char** argv)
{
    assert(argc == 3);
    auto base = load(argv[1]), symbols = load(argv[2]);
    font_base_addr = base.data(); reader_font_base_addr = symbols.data();
    const char* inputs[] = {u8"‘", u8"’", u8"‚", u8"“", u8"”", u8"„", u8"‐", u8"‑"};
    const char* outputs[] = {"'", "'", "'", "\"", "\"", "\"", "-", "-"};
    for(int i = 0; i < 8; ++i) {
        auto page = layout(inputs[i]);
        assert(render(page.lines[0].text) == render(outputs[i]));
        assert(font_width(page.lines[0].text) == font_width(outputs[i]));
        std::string original, shown;
        for(int n = 0; n < 10000; ++n) { original += inputs[i]; shown += outputs[i]; }
        auto a = layout(original), b = layout(shown);
        assert(a.line_count == b.line_count && a.next_offset == 3 * b.next_offset);
        for(int n = 0; n < a.line_count; ++n) {
            assert(!std::strcmp(a.lines[n].text, b.lines[n].text));
            assert(font_width(a.lines[n].text) <= 224);
        }
        reader::MemorySource source(reinterpret_cast<const unsigned char*>(original.data()), original.size());
        reader::PageHistory history{};
        reader::Page resumed{}, next{}, back{};
        assert(reader::open_page_at(source, a.next_offset, reader::default_settings(), width, history, resumed));
        assert(resumed.start_offset == a.next_offset);
        assert(reader::next_page(source, reader::default_settings(), width, history, resumed, next));
        assert(reader::previous_page(source, reader::default_settings(), width, history, back));
        assert(back.start_offset == resumed.start_offset && back.next_offset == resumed.next_offset);
        // Emulate a saved snapshot whose boundaries came from another glyph width.
        // Opening the byte anchor resets history; rebuilding with current widths must
        // reach/overlap that exact anchor, never leave an unread gap before it.
        const uint32_t anchor = a.next_offset + 3;
        history.count = 1;
        history.offsets[0] = 0;
        assert(reader::previous_page(source, reader::default_settings(), width, history, back));
        assert(back.next_offset < anchor); // Persisted history is NOT self-validating.
        history.count = 1;
        reader::PageHistoryRebuild rebuild{};
        assert(reader::open_page_at(source, anchor, reader::default_settings(), width, history, resumed));
        assert(history.count == 0 && history.lazy && resumed.start_offset == anchor);
        reader::begin_history_rebuild(resumed.start_offset, rebuild);
        while(rebuild.state == reader::HistoryRebuildState::BUILDING)
            reader::step_history_rebuild(source, reader::default_settings(), width, rebuild);
        assert(reader::adopt_rebuilt_history(rebuild, history));
        assert(reader::previous_page(source, reader::default_settings(), width, history, back));
        assert(back.start_offset < anchor && back.next_offset >= anchor);
        assert(reader::next_page(source, reader::default_settings(), width, history, back, next));
        assert(next.start_offset == back.next_offset); // Contiguous new-layout forward navigation.
        std::printf("punctuation %s -> %s: %u -> %u px, rebuilt anchor %u without gap\n",
                    inputs[i], outputs[i], font_width(inputs[i]), font_width(outputs[i]), anchor);
    }
    // U+2011 stays inside its word: overflow rewinds to the preceding space, not the hyphen.
    std::string prefix(24, 'W');
    while(font_width(prefix.c_str()) + font_width(" ab-") > 224) prefix.pop_back();
    const auto no_break = layout(prefix + u8" ab‑cdefghijklmnop");
    assert(!std::strcmp(no_break.lines[0].text, prefix.c_str()));
    assert(std::strstr(no_break.lines[1].text, "ab-cdefghijklmnop"));

    Pixels actual{}, expected{};
    reader::draw_credits(reinterpret_cast<uint8_t*>(actual.data()));
    const char* lines[] = {"Made by Halim Jarrar", "(C) 2026", "halim-jarrar.de", "monday@halim-jarrar.de"};
    for(int i = 0; i < 4; ++i) {
        const unsigned w = font_width(lines[i]);
        assert(w > 0 && w <= 224);
        draw_text_idx8_bus16_range(lines[i], reinterpret_cast<uint8_t*>(expected.data()) +
                                  (40 + i * 22) * 240 + (240 - w) / 2, 0, w, 240, 1);
        std::printf("credits line %d: %u px\n", i, w);
    }
    assert(actual == expected); // Exact full framebuffer, real bus-safe compositor and fonts.
    assert(actual != Pixels{});
    for(int i = 0; i < 4; ++i) assert(!std::strcmp(reader::credits_lines[0][i], lines[i]));
    assert(!reader::credits_lines[0][4][0] && !reader::credits_lines[0][5][0]);
    std::string notices;
    for(int p = 1; p < reader::CREDITS_PAGE_COUNT; ++p) {
        actual = {}; expected = {};
        reader::draw_credits(reinterpret_cast<uint8_t*>(actual.data()), p);
        for(int i = 0; i < reader::CREDITS_LINES; ++i) {
            const char* text = reader::credits_lines[p][i];
            const unsigned w = font_width(text); assert(w <= 224);
            for(const char* ch=text; *ch; ++ch) assert(*ch >= 32 && *ch <= 126);
            notices += text; notices += '\n';
            draw_text_idx8_bus16_range(text, reinterpret_cast<uint8_t*>(expected.data()) +
                (30 + i * 16) * 240 + (240 - w) / 2, 0, w, 240, 1);
        }
        assert(actual == expected && actual != Pixels{});
    }
    for(const char* required : {"Ghoulam Regular", "Imad AlFil / mloukhiyye", "CC BY 4.0",
            "SuperFW", "David Guillen Fandos", "GPL v3 or later", "UNSCII by Viznut",
            "unscii-16-full: GPL", "Fixedsys Excelsior", "public domain", "GNU Unifont / Hangul",
            "Roman Czyborra, Paul Hardy", "GPL v2+ with font exception", "Butano", "Gustavo Valiente",
            "zlib", "devkitARM / devkitPro", "FatFs (C) 2022 ChaN", "miniz - MIT", "Rich Geldreich",
            "RAD Game Tools / Valve", "gba-vocab-trainer-CC"}) assert(notices.find(required) != std::string::npos);
    assert(reader::CONTROLS_PAGE_COUNT == 6);
    assert(std::strstr(reader::controls_titles[5], "Arabic"));
    for(int p = 0; p < reader::CONTROLS_PAGE_COUNT; ++p) {
        actual = {}; expected = {};
        reader::draw_controls(reinterpret_cast<uint8_t*>(actual.data()), p);
        for(int i = 0; i < reader::CONTROLS_LINES; ++i) {
            const char* text = reader::controls_lines[p][i];
            assert(font_width(text) <= 224);
            for(const char* c = text; *c; ++c) assert(*c >= 32 && *c <= 126);
            draw_text_idx8_bus16_range(text, reinterpret_cast<uint8_t*>(expected.data()) +
                                      (30 + i * 16) * 240 + 8, 0, 224, 240, 1);
        }
        assert(actual == expected && actual != Pixels{});
    }
    std::puts("PASS: real renderer punctuation pixels, width/wrap, offsets, credits/controls framebuffer");
}
