#include "reader_core.h"

#include <cstring>

namespace reader {

Settings default_settings() { return { 2, 8, 8 }; }

void clamp_settings(Settings& s)
{
    if(s.line_spacing > MAX_LINE_SPACING) s.line_spacing = MAX_LINE_SPACING;
    if(s.top_margin > MAX_MARGIN) s.top_margin = MAX_MARGIN;
    if(s.bottom_margin > MAX_MARGIN) s.bottom_margin = MAX_MARGIN;
}

void adjust_setting(Settings& s, SettingField field, int delta)
{
    int* unused = nullptr;
    (void) unused;
    uint8_t* value = field == SettingField::LINE_SPACING ? &s.line_spacing :
                     field == SettingField::TOP_MARGIN ? &s.top_margin : &s.bottom_margin;
    int maximum = field == SettingField::LINE_SPACING ? MAX_LINE_SPACING : MAX_MARGIN;
    int result = int(*value) + delta;
    if(result < 0) result = 0;
    if(result > maximum) result = maximum;
    *value = uint8_t(result);
}

bool MemorySource::byte_at(uint32_t offset, unsigned char& value) const
{
    if(offset >= _size) return false;
    value = _data[offset];
    return true;
}

struct Decoded {
    uint32_t code;
    uint8_t bytes[4];
    int count;
    int consumed;
    bool source_ok;
};

static Decoded decode(const ByteSource& source, uint32_t offset)
{
    Decoded d{ '?', {'?'}, 1, 1, true };
    unsigned char a = 0;
    if(! source.byte_at(offset, a)) return { 0, {0}, 0, 0, false };
    if(a < 0x80) return { a, {a}, 1, 1, true };
    int count = (a >= 0xC2 && a <= 0xDF) ? 2 : (a >= 0xE0 && a <= 0xEF) ? 3 :
                (a >= 0xF0 && a <= 0xF4) ? 4 : 0;
    if(! count) return d;
    d.bytes[0] = a;
    for(int i = 1; i < count; ++i) {
        unsigned char c = 0;
        if(! source.byte_at(offset + uint32_t(i), c)) {
            if(offset + uint32_t(i) < source.size()) d.source_ok = false;
            return d;
        }
        if((c & 0xC0) != 0x80) return d;
        d.bytes[i] = c;
    }
    uint32_t cp = count == 2 ? (a & 0x1F) : count == 3 ? (a & 0x0F) : (a & 0x07);
    for(int i = 1; i < count; ++i) cp = (cp << 6) | (d.bytes[i] & 0x3F);
    if((count == 3 && cp >= 0xD800 && cp <= 0xDFFF) ||
       (count == 3 && cp < 0x800) || (count == 4 && (cp < 0x10000 || cp > 0x10FFFF))) return d;
    const bool arabic = (cp >= 0x0600 && cp <= 0x06FF) ||
                        (cp >= 0x0750 && cp <= 0x077F) ||
                        (cp >= 0x08A0 && cp <= 0x08FF) ||
                        (cp >= 0xFB50 && cp <= 0xFDFF) ||
                        (cp >= 0xFE70 && cp <= 0xFEFF);
    if(arabic) {
        d.code = '?';
        d.bytes[0] = '?';
        d.count = 1;
        d.consumed = count;
        return d;
    }
    d.code = cp;
    d.count = count;
    d.consumed = count;
    return d;
}

static bool make_line(const ByteSource& source, uint32_t& cursor, GlyphWidth width_fn, PageLine& line,
                      bool& source_ok)
{
    const int max_width = SCREEN_WIDTH - BODY_SIDE_MARGIN * 2;
    uint32_t start = cursor;
    uint32_t last_space_next = 0;
    int last_space_out = -1;
    int out = 0;
    int width = 0;
    line.paragraph_break = false;
    line.text[0] = 0;

    while(cursor < source.size()) {
        unsigned char raw = 0;
        if(! source.byte_at(cursor, raw)) { source_ok = false; return false; }
        if(raw == '\r' || raw == '\n') {
            cursor++;
            if(raw == '\r') {
                unsigned char lf = 0;
                if(cursor < source.size() && ! source.byte_at(cursor, lf)) {
                    source_ok = false;
                    return false;
                }
                if(cursor < source.size() && lf == '\n') cursor++;
            }
            line.paragraph_break = out == 0;
            break;
        }
        Decoded d = decode(source, cursor);
        if(! d.source_ok) { source_ok = false; return false; }
        int glyph_width = width_fn ? width_fn(d.code) : 8;
        if(glyph_width < 1) glyph_width = 8;
        if(width + glyph_width > max_width && out > 0) {
            if(last_space_out >= 0) {
                out = last_space_out;
                cursor = last_space_next;
                unsigned char c = 0;
                while(cursor < source.size()) {
                    if(! source.byte_at(cursor, c)) { source_ok = false; return false; }
                    if(c != ' ') break;
                    cursor++;
                }
            }
            break;
        }
        if(out + d.count >= PAGE_LINE_BYTES) break;
        for(int i = 0; i < d.count; ++i) line.text[out++] = char(d.bytes[i]);
        cursor += uint32_t(d.consumed);
        width += glyph_width;
        if(d.code == ' ') {
            last_space_out = out - 1;
            last_space_next = cursor;
        }
    }
    while(out > 0 && line.text[out - 1] == ' ') --out;
    line.text[out] = 0;
    return cursor > start;
}

bool layout_page(const ByteSource& source, uint32_t offset, const Settings& input,
                 GlyphWidth glyph_width, Page& page)
{
    Settings settings = input;
    clamp_settings(settings);
    if(offset > source.size()) return false;
    if(offset == 0 && source.size() >= 3) {
        unsigned char a = 0, b = 0, c = 0;
        if(! source.byte_at(0, a) || ! source.byte_at(1, b) || ! source.byte_at(2, c)) return false;
        if(a == 0xEF && b == 0xBB && c == 0xBF) offset = 3;
    }
    Page result{};
    result.start_offset = offset;
    uint32_t cursor = offset;
    int line_height = FONT_HEIGHT + settings.line_spacing;
    int capacity = (160 - settings.top_margin - settings.bottom_margin) / line_height;
    if(capacity < 1) capacity = 1;
    if(capacity > PAGE_MAX_LINES) capacity = PAGE_MAX_LINES;
    bool source_ok = true;
    while(cursor < source.size() && result.line_count < capacity) {
        if(! make_line(source, cursor, glyph_width, result.lines[result.line_count], source_ok)) break;
        ++result.line_count;
    }
    if(! source_ok) return false;
    result.next_offset = cursor;
    result.eof = cursor >= source.size();
    if(result.line_count == 0 && ! result.eof) return false;
    page = result;
    return true;
}

bool open_first_page(const ByteSource& source, const Settings& settings, GlyphWidth glyph_width,
                     PageHistory& history, Page& page)
{
    history = {};
    return layout_page(source, 0, settings, glyph_width, page);
}

static void remember_page(PageHistory& history, uint32_t offset)
{
    if(history.count == PAGE_HISTORY_MAX) {
        std::memmove(history.offsets, history.offsets + 1, sizeof(uint32_t) * (PAGE_HISTORY_MAX - 1));
        --history.count;
    }
    history.offsets[history.count++] = offset;
}

bool open_page_at(const ByteSource& source, uint32_t offset, const Settings& settings,
                  GlyphWidth glyph_width, PageHistory& history, Page& page)
{
    history = {};
    if(offset == 0) return layout_page(source, 0, settings, glyph_width, page);
    if(offset >= source.size()) return false;

    Page scan{};
    if(! layout_page(source, 0, settings, glyph_width, scan)) return false;
    while(scan.start_offset < offset) {
        remember_page(history, scan.start_offset);
        if(scan.eof || scan.next_offset <= scan.start_offset || scan.next_offset >= offset) break;
        if(! layout_page(source, scan.next_offset, settings, glyph_width, scan)) return false;
    }
    return layout_page(source, offset, settings, glyph_width, page);
}

bool next_page(const ByteSource& source, const Settings& settings, GlyphWidth glyph_width,
               PageHistory& history, const Page& current, Page& next)
{
    if(current.eof || current.next_offset <= current.start_offset) return false;
    if(! layout_page(source, current.next_offset, settings, glyph_width, next)) return false;
    remember_page(history, current.start_offset);
    return true;
}

bool previous_page(const ByteSource& source, const Settings& settings, GlyphWidth glyph_width,
                   PageHistory& history, Page& previous)
{
    if(history.count <= 0) return false;
    uint32_t offset = history.offsets[history.count - 1];
    if(! layout_page(source, offset, settings, glyph_width, previous)) return false;
    --history.count;
    return true;
}

}
