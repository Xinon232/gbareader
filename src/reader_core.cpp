#include "reader_core.h"
#include "reader_arabic.h"

#ifdef __DEVKITARM__
namespace arabic { Line scratch __attribute__((section(".sbss"))); }
#endif

#include <cstring>

namespace reader {

bool ByteSource::read_range(uint32_t offset, unsigned char* output, uint32_t count) const
{
    if(!output || offset > size() || count > size() - offset) return false;
    for(uint32_t index = 0; index < count; ++index)
        if(!byte_at(offset + index, output[index])) return false;
    return true;
}

bool ByteSource::export_text(TextSink sink, void* context) const
{
    if(!sink) return false;
    unsigned char block[512];
    for(uint32_t at = 0; at < size();) {
        uint32_t take = size() - at;
        if(take > sizeof(block)) take = sizeof(block);
        if(!read_range(at, block, take) || !sink(context, block, take)) return false;
        at += take;
    }
    return true;
}

Settings default_settings() { return { 1, ParagraphGap::FULL }; }

bool same_settings(const Settings& a, const Settings& b)
{
    return a.line_spacing == b.line_spacing && a.paragraph_gap == b.paragraph_gap &&
           a.arabic_shaping == b.arabic_shaping;
}

void clamp_settings(Settings& s)
{
    if(s.line_spacing > MAX_LINE_SPACING) s.line_spacing = MAX_LINE_SPACING;
    if(uint8_t(s.paragraph_gap) >= PARAGRAPH_GAP_COUNT) s.paragraph_gap = ParagraphGap::FULL;
}

static int clamp_int(int value, int minimum, int maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

void adjust_setting(Settings& s, SettingField field, int delta)
{
    if(field == SettingField::LINE_SPACING)
        s.line_spacing = uint8_t(clamp_int(int(s.line_spacing) + delta, MIN_LINE_SPACING, MAX_LINE_SPACING));
    else
        s.paragraph_gap = ParagraphGap(clamp_int(int(s.paragraph_gap) + delta, 0, PARAGRAPH_GAP_COUNT - 1));
}

const char* paragraph_gap_name(ParagraphGap gap)
{
    switch(gap) {
    case ParagraphGap::NONE: return "None";
    case ParagraphGap::SMALL: return "Small";
    case ParagraphGap::HALF: return "Half";
    default: return "Full";
    }
}

int paragraph_gap_pixels(const Settings& s)
{
    switch(s.paragraph_gap) {
    case ParagraphGap::NONE: return 0;
    case ParagraphGap::SMALL: return 3;
    case ParagraphGap::HALF: return FONT_HEIGHT / 2;
    default: return FONT_HEIGHT + s.line_spacing; // One empty line.
    }
}

static int text_height(int lines, int spacing)
{
    return lines * FONT_HEIGHT + (lines - 1) * spacing;
}

int lines_per_page(const Settings& input)
{
    Settings s = input;
    clamp_settings(s);
    int lines = 1;
    while(lines < PAGE_MAX_LINES && text_height(lines + 1, s.line_spacing) <= PAGE_HEIGHT) ++lines;
    return lines;
}

int page_top(const Settings& input)
{
    Settings s = input;
    clamp_settings(s);
    return (PAGE_HEIGHT - text_height(lines_per_page(s), s.line_spacing)) / 2;
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

static Decoded decode(const ByteSource& source, uint32_t offset, unsigned char first)
{
    Decoded d{ '?', {'?'}, 1, 1, true };
    unsigned char a = first;
    if(a < 0x80) return { a, {a}, 1, 1, true };
    int count = (a >= 0xC2 && a <= 0xDF) ? 2 : (a >= 0xE0 && a <= 0xEF) ? 3 :
                (a >= 0xF0 && a <= 0xF4) ? 4 : 0;
    if(! count) return d;
    uint8_t bytes[4] = {a, 0, 0, 0};
    for(int i = 1; i < count; ++i) {
        unsigned char c = 0;
        // A failed lazy read can invalidate the source and clear its size.
        const bool in_bounds = offset + uint32_t(i) < source.size();
        if(! source.byte_at(offset + uint32_t(i), c)) {
            if(in_bounds) d.source_ok = false;
            return d;
        }
        if((c & 0xC0) != 0x80) return d;
        bytes[i] = c;
    }
    uint32_t cp = count == 2 ? (a & 0x1F) : count == 3 ? (a & 0x0F) : (a & 0x07);
    for(int i = 1; i < count; ++i) cp = (cp << 6) | (bytes[i] & 0x3F);
    if((count == 3 && cp >= 0xD800 && cp <= 0xDFFF) ||
       (count == 3 && cp < 0x800) || (count == 4 && (cp < 0x10000 || cp > 0x10FFFF))) return d;
    const bool apostrophe = cp == 0x2018 || cp == 0x2019 || cp == 0x201A;
    const bool quote = cp == 0x201C || cp == 0x201D || cp == 0x201E;
    const bool hyphen = cp == 0x2010 || cp == 0x2011 || cp == 0x2212;
    if(apostrophe || quote || hyphen || (cp >= 0xFF01 && cp <= 0xFF5E)) {
        // Display bytes and widths only; advance by the original UTF-8 length.
        // Only spaces are wrap opportunities, so U+2011 gains no hyphen break.
        const uint32_t ascii = apostrophe ? '\'' : quote ? '"' : hyphen ? '-' : cp - 0xFEE0;
        d.code = ascii;
        d.bytes[0] = static_cast<uint8_t>(ascii);
        d.count = 1;
        d.consumed = count;
        return d;
    }
    std::memcpy(d.bytes, bytes, count);
    d.code = cp;
    d.count = count;
    d.consumed = count;
    return d;
}

static bool make_line(const ByteSource& source, uint32_t& cursor, GlyphWidth width_fn, PageLine& line,
                      bool& source_ok, bool arabic_shaping)
{
    const int max_width = SCREEN_WIDTH - BODY_SIDE_MARGIN * 2;
    uint32_t start = cursor;
    uint32_t last_space_next = 0;
    int last_space_out = -1;
    int out = 0;
    int width = 0;
    bool has_arabic = false;
    line.paragraph_break = false;
    line.text[0] = 0;

    while(cursor < source.size()) {
        unsigned char raw = 0;
        if(! source.byte_at(cursor, raw)) { source_ok = false; return false; }
        if(raw == '\r' || raw == '\n') {
            int newlines = 0;
            do {
                const unsigned char newline = raw;
                ++cursor;
                if(newline == '\r' && cursor < source.size()) {
                    unsigned char lf = 0;
                    if(! source.byte_at(cursor, lf)) { source_ok = false; return false; }
                    if(lf == '\n') ++cursor;
                }
                ++newlines;
                if(cursor >= source.size()) break;
                if(! source.byte_at(cursor, raw)) { source_ok = false; return false; }
            } while(raw == '\r' || raw == '\n');
            line.paragraph_break = newlines >= 2;
            break;
        }
        if(raw == ' ') {
            uint32_t run_end = cursor + 1;
            unsigned char next = 0;
            while(run_end < source.size()) {
                if(! source.byte_at(run_end, next)) { source_ok = false; return false; }
                if(next != ' ') break;
                ++run_end;
            }
            if(run_end - cursor >= 3) {
                cursor = run_end;
                if(out == 0 || line.text[out - 1] == ' ') continue;
                int space_width = width_fn ? width_fn(' ') : 8;
                if(space_width < 1) space_width = 8;
                if(width + space_width > max_width) break;
                if(out + 1 >= PAGE_LINE_BYTES) break;
                line.text[out++] = ' ';
                width += space_width;
                last_space_out = out - 1;
                last_space_next = cursor;
                continue;
            }
        }
        Decoded d = decode(source, cursor, raw);
        if(! d.source_ok) { source_ok = false; return false; }
        int glyph_width = width_fn ? width_fn(d.code) : 8;
        if(glyph_width < 1) glyph_width = 8;
        if(out + d.count >= PAGE_LINE_BYTES) break;
        has_arabic = has_arabic || (arabic_shaping && arabic::script(d.code));
        int candidate_width = width + glyph_width;
        if(has_arabic) {
            // Bounded prefix (255 bytes): re-shape contextual forms and lam-alef
            // before accepting the next logical character. Never alter offsets.
            for(int i = 0; i < d.count; ++i) line.text[out + i] = char(d.bytes[i]);
            line.text[out + d.count] = 0;
            candidate_width = arabic_extent(shape_reader_line(line.text, width_fn));
            line.text[out] = 0;
        }
        if(candidate_width > max_width && out > 0) {
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
        width = candidate_width;
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
    const int height_limit = text_height(lines_per_page(settings), settings.line_spacing);
    int used_height = 0;
    bool source_ok = true;
    while(cursor < source.size() && result.line_count < PAGE_MAX_LINES) {
        int gap_before = 0;
        if(result.line_count > 0) {
            gap_before = settings.line_spacing;
            if(result.lines[result.line_count - 1].paragraph_break)
                gap_before += paragraph_gap_pixels(settings);
        }
        if(used_height + gap_before + FONT_HEIGHT > height_limit) break;
        if(!make_line(source, cursor, glyph_width, result.lines[result.line_count], source_ok, settings.arabic_shaping)) break;
        used_height += gap_before + FONT_HEIGHT;
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
    if(history.count < PAGE_HISTORY_MAX) {
        history.offsets[(history.head + history.count) % PAGE_HISTORY_MAX] = offset;
        ++history.count;
    } else {
        history.offsets[history.head] = offset;
        history.head = (history.head + 1) % PAGE_HISTORY_MAX;
    }
}

// Start of the paragraph holding `target`, or the next word when that paragraph
// starts too far back; never inside a UTF-8 sequence.
static bool snap_to_break(const ByteSource& source, uint32_t target, uint32_t& offset)
{
    constexpr uint32_t SEARCH_LIMIT = 1024;
    const uint32_t size = source.size();
    if(size == 0) return false;
    if(target >= size) target = size - 1;
    unsigned char value = 0;
    // Step off line ends so a target on a break belongs to the text before it.
    uint32_t at = target;
    while(at > 0 && target - at < SEARCH_LIMIT) {
        if(!source.byte_at(at, value)) return false;
        if(value != '\n' && value != '\r') break;
        --at;
    }
    const uint32_t lowest = at > SEARCH_LIMIT ? at - SEARCH_LIMIT : 0;
    for(uint32_t p = at; p > lowest; --p) {
        if(!source.byte_at(p - 1, value)) return false;
        if(value == '\n') { offset = p; return true; }
    }
    if(lowest == 0) { offset = 0; return true; }
    // Long paragraph: start at the next word instead.
    for(uint32_t p = target; p < size && p - target < SEARCH_LIMIT; ++p) {
        if(!source.byte_at(p, value)) return false;
        if((value == ' ' || value == '\n') && p + 1 < size) { offset = p + 1; return true; }
    }
    // No break nearby: at least avoid starting inside a UTF-8 sequence.
    uint32_t p = target;
    while(p > 0 && source.byte_at(p, value) && (value & 0xC0) == 0x80) --p;
    offset = p;
    return true;
}

bool open_page_at(const ByteSource& source, uint32_t offset, const Settings& settings,
                  GlyphWidth glyph_width, PageHistory& history, Page& page)
{
    history = {};
    if(offset >= source.size()) return false;
    if(!layout_page(source, offset, settings, glyph_width, page)) return false;
    history.lazy = offset > 0;
    history.lazy_anchor = offset;
    return true;
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
    const int index = (history.head + history.count - 1) % PAGE_HISTORY_MAX;
    uint32_t offset = history.offsets[index];
    if(! layout_page(source, offset, settings, glyph_width, previous)) return false;
    --history.count;
    return true;
}

void begin_history_rebuild(uint32_t anchor, PageHistoryRebuild& rebuild)
{
    rebuild = {};
    rebuild.anchor = anchor;
    rebuild.state = anchor ? HistoryRebuildState::BUILDING : HistoryRebuildState::READY;
}

HistoryRebuildState step_history_rebuild(const ByteSource& source, const Settings& settings,
                                         GlyphWidth glyph_width, PageHistoryRebuild& rebuild)
{
    if(rebuild.state != HistoryRebuildState::BUILDING) return rebuild.state;

    if(!rebuild.initialized) {
        uint32_t start = 0;
        if(rebuild.anchor > HISTORY_REBUILD_WINDOW &&
           (!snap_to_break(source, rebuild.anchor - HISTORY_REBUILD_WINDOW, start) ||
            start >= rebuild.anchor))
            start = 0;
        if(!layout_page(source, start, settings, glyph_width, rebuild.scan)) {
            rebuild.state = HistoryRebuildState::FAILED;
            return rebuild.state;
        }
        rebuild.initialized = true;
    } else {
        if(rebuild.scan.eof || rebuild.scan.next_offset <= rebuild.scan.start_offset) {
            rebuild.state = HistoryRebuildState::FAILED;
            return rebuild.state;
        }
        if(rebuild.scan.next_offset >= rebuild.anchor) {
            rebuild.state = HistoryRebuildState::READY;
            return rebuild.state;
        }
        const uint32_t next_offset = rebuild.scan.next_offset;
        if(!layout_page(source, next_offset, settings, glyph_width, rebuild.scan)) {
            rebuild.state = HistoryRebuildState::FAILED;
            return rebuild.state;
        }
    }

    if(rebuild.scan.start_offset < rebuild.anchor)
        remember_page(rebuild.rebuilt, rebuild.scan.start_offset);
    if(rebuild.scan.eof || rebuild.scan.next_offset >= rebuild.anchor)
        rebuild.state = HistoryRebuildState::READY;
    return rebuild.state;
}

void begin_page_count(uint32_t target, PageCount& count)
{
    count = {};
    count.target = target;
    count.checkpoint_stride = 1;
    count.state = target ? HistoryRebuildState::BUILDING : HistoryRebuildState::READY;
}

void retarget_page_count(uint32_t target, PageCount& count)
{
    if(count.checkpoint_stride == 0) { begin_page_count(target, count); return; }
    uint32_t best_offset = 0, best_pages = 0;
    for(int i = 0; i < count.checkpoint_count; ++i)
        if(count.checkpoint_offsets[i] < target && count.checkpoint_offsets[i] >= best_offset) {
            best_offset = count.checkpoint_offsets[i];
            best_pages = count.checkpoint_pages[i];
        }
    // The page being scanned is the furthest counted page (index pages - 1).
    if(count.initialized && count.state != HistoryRebuildState::FAILED && count.pages &&
       count.scan.start_offset < target && count.scan.start_offset >= best_offset) {
        best_offset = count.scan.start_offset;
        best_pages = count.pages - 1;
    }
    count.target = target;
    count.start_offset = best_offset;
    count.pages = best_pages;
    count.initialized = false;
    count.state = target ? HistoryRebuildState::BUILDING : HistoryRebuildState::READY;
    if(!target) count.pages = 0;
}

static void remember_checkpoint(PageCount& count, uint32_t offset, uint32_t index)
{
    if(index % count.checkpoint_stride) return;
    for(int i = 0; i < count.checkpoint_count; ++i)
        if(count.checkpoint_offsets[i] == offset) return;
    if(count.checkpoint_count == PAGE_COUNT_CHECKPOINTS) {
        // Full: keep every other checkpoint and space new ones twice as far apart.
        int kept = 0;
        count.checkpoint_stride *= 2;
        for(int i = 0; i < count.checkpoint_count; ++i)
            if(count.checkpoint_pages[i] % count.checkpoint_stride == 0) {
                count.checkpoint_offsets[kept] = count.checkpoint_offsets[i];
                count.checkpoint_pages[kept] = count.checkpoint_pages[i];
                ++kept;
            }
        count.checkpoint_count = kept;
        if(index % count.checkpoint_stride || kept == PAGE_COUNT_CHECKPOINTS) return;
    }
    count.checkpoint_offsets[count.checkpoint_count] = offset;
    count.checkpoint_pages[count.checkpoint_count] = index;
    ++count.checkpoint_count;
}

HistoryRebuildState step_page_count(const ByteSource& source, const Settings& settings,
                                    GlyphWidth glyph_width, PageCount& count)
{
    if(count.state != HistoryRebuildState::BUILDING) return count.state;
    const uint32_t offset = count.initialized ? count.scan.next_offset : count.start_offset;
    if(count.initialized && (count.scan.eof || offset <= count.scan.start_offset)) {
        count.state = HistoryRebuildState::FAILED;
        return count.state;
    }
    if(!layout_page(source, offset, settings, glyph_width, count.scan)) {
        count.state = HistoryRebuildState::FAILED;
        return count.state;
    }
    count.initialized = true;
    if(count.checkpoint_stride) remember_checkpoint(count, count.scan.start_offset, count.pages);
    if(count.scan.start_offset < count.target) ++count.pages;
    if(count.scan.eof || count.scan.next_offset >= count.target)
        count.state = HistoryRebuildState::READY;
    return count.state;
}

int page_count_estimate(const PageCount& count)
{
    if(count.state == HistoryRebuildState::READY) return int(count.pages);
    if(count.state != HistoryRebuildState::BUILDING || !count.initialized || !count.pages ||
       count.scan.next_offset == 0) return -1;
    return int(uint64_t(count.pages) * count.target / count.scan.next_offset);
}

int page_percent(const Page& page, uint32_t source_size)
{
    if(page.eof || source_size == 0) return 100;
    return int(uint64_t(page.start_offset) * 100 / source_size);
}

bool percent_offset(const ByteSource& source, int percent, uint32_t& offset)
{
    const uint32_t size = source.size();
    if(size == 0) return false;
    if(percent <= 0) { offset = 0; return true; }
    const uint32_t target = percent >= 100 ? size - 1 : uint32_t(uint64_t(size) * uint32_t(percent) / 100);
    return snap_to_break(source, target, offset);
}

uint32_t history_rebuild_anchor(const PageHistory& history, uint32_t current)
{
    return history.count > 0 ? history.offsets[history.head] : current;
}

bool prepend_rebuilt_history(PageHistoryRebuild& rebuild, PageHistory& history)
{
    if(rebuild.state != HistoryRebuildState::READY || history.count <= 0 ||
       rebuild.anchor != history.offsets[history.head]) return false;
    PageHistory& merged = rebuild.rebuilt;
    for(int i = 0; i < history.count; ++i)
        remember_page(merged, history.offsets[(history.head + i) % PAGE_HISTORY_MAX]);
    return adopt_rebuilt_history(rebuild, history);
}

bool adopt_rebuilt_history(PageHistoryRebuild& rebuild, PageHistory& history)
{
    if(rebuild.state != HistoryRebuildState::READY) return false;
    history = rebuild.rebuilt;
    history.lazy = false;
    history.lazy_anchor = 0;
    rebuild.state = HistoryRebuildState::IDLE;
    return true;
}

}
