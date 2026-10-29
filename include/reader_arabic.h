#pragma once
#include "arabic_text.h"
#include "reader_core.h"

namespace reader {
// Advance plus visible right bearing: wrapping and painting use one geometry.
inline int arabic_extent(const arabic::Line& line)
{
    if(!line.valid) return SCREEN_WIDTH;
    int extent = line.width;
    for(int i = 0; i < line.count; ++i) {
        const auto& item = line.items[i];
        if(item.glyph) {
            const auto& glyph = arabic::glyphs[item.glyph];
            const int right = item.x + glyph.left + glyph.width;
            if(right > extent) extent = right;
        }
    }
    return extent;
}
inline const arabic::Line& shape_reader_line(const char* text, GlyphWidth width)
{
    return arabic::shape(text, [width](const char* ch) {
        int at = 0;
        const unsigned cp = arabic::decode(ch, int(std::strlen(ch)), at);
        const int result = width ? width(cp) : 8;
        return result > 0 ? result : 8;
    });
}
}
