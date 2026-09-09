#pragma once
#include "reader_arabic.h"
extern "C" {
#include "font_render.h"
}
namespace reader {
inline int body_glyph_width(uint32_t cp)
{
    char ch[5]; arabic::encode(cp, ch);
    return int(font_width(ch));
}
// Caller supplies the first pixel of a 224x16 body row with 240-byte pitch.
// All device writes are halfword read/modify/write, including odd glyph x.
inline void draw_body_line(const char* text, uint8_t* pixels)
{
    bool shaped = false;
    const int n = int(std::strlen(text));
    for(int p = 0; p < n;) {
        const int begin = p;
        const unsigned cp = arabic::decode(text, n, p);
        char canonical[5]; arabic::encode(cp, canonical);
        const int count = int(std::strlen(canonical));
        // The legacy painter assumes complete UTF-8 and can step beyond NUL.
        // Route malformed body bytes through the bounded replacement decoder.
        if(arabic::script(cp) || count != p - begin ||
                std::memcmp(text + begin, canonical, count)) { shaped = true; break; }
    }
    if(!shaped) {
        draw_text_idx8_bus16_range(text, pixels, 0, 224, 240, 1);
        return;
    }
    const auto& line = shape_reader_line(text, body_glyph_width);
    if(!line.valid) return;
    bool rtl = false;
    for(int p = 0; p < n;) {
        int d = arabic::direction(arabic::decode(text, n, p));
        if(d == 0 || d == 1) { rtl = d == 1; break; }
    }
    const int extent = arabic_extent(line);
    const int origin = rtl && extent < 224 ? 224 - extent : 0;
    for(int i = 0; i < line.count; ++i) {
        const auto& item = line.items[i];
        if(!item.glyph) {
            const int x = origin + item.x;
            if(item.advance && x >= 0 && x < 224) {
                char ch[5]; arabic::encode(item.code, ch);
                draw_text_idx8_bus16_range(ch, pixels + x, 0, 224 - x, 240, 1);
            }
            continue;
        }
        const auto& glyph = arabic::glyphs[item.glyph];
        for(int y = 0; y < glyph.height; ++y) {
            const int dy = glyph.top + y;
            if(dy < 0 || dy >= 16) continue;
            for(int x = 0; x < glyph.width; ++x) {
                const int dx = origin + item.x + glyph.left + x;
                if(dx < 0 || dx >= 224 || !(arabic::bitmap[glyph.offset + y] & (1u << x))) continue;
                auto* pair = reinterpret_cast<volatile uint16_t*>(pixels + dy * 240 + (dx & ~1));
                const int shift = (dx & 1) * 8;
                *pair = uint16_t((*pair & ~(255u << shift)) | (1u << shift));
            }
        }
    }
}
}
