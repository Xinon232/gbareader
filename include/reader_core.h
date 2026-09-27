#pragma once

#include <cstddef>
#include <cstdint>

namespace reader {

constexpr int SCREEN_WIDTH = 240;
constexpr int BODY_SIDE_MARGIN = 8;
constexpr int FONT_HEIGHT = 16;
constexpr int PAGE_HEIGHT = 160;
constexpr int MIN_LINE_SPACING = 0;
constexpr int MAX_LINE_SPACING = 4;
constexpr int PAGE_MAX_LINES = 10;
constexpr int PAGE_LINE_BYTES = 256;
constexpr int PAGE_HISTORY_MAX = 64;

// Extra space after a paragraph break, ordered from tightest to widest.
enum class ParagraphGap : uint8_t { NONE, SMALL, HALF, FULL };
constexpr int PARAGRAPH_GAP_COUNT = 4;

struct Settings {
    uint8_t line_spacing;
    ParagraphGap paragraph_gap;
    bool arabic_shaping = false; // Per-document; never inherited from another open.
};

enum class SettingField : uint8_t { LINE_SPACING, PARAGRAPH_GAP };
constexpr int SETTING_FIELD_COUNT = 2;

Settings default_settings();
bool same_settings(const Settings& a, const Settings& b);
void clamp_settings(Settings& settings);
void adjust_setting(Settings& settings, SettingField field, int delta);
const char* paragraph_gap_name(ParagraphGap gap);
// Pixels added after a paragraph break, on top of the normal line spacing.
int paragraph_gap_pixels(const Settings& settings);
// Lines of unbroken text that fit on a page; paragraph breaks can only reduce it.
int lines_per_page(const Settings& settings);
// First body row: a full page of unbroken text is centered vertically.
int page_top(const Settings& settings);

using TextSink = bool (*)(void* context, const unsigned char* bytes, uint32_t count);

class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual uint32_t size() const = 0;
    virtual bool byte_at(uint32_t offset, unsigned char& value) const = 0;
    // Derived text windows must not bypass a latched/replaced backing source.
    virtual bool read_ready() const { return true; }
    // Bulk reads let cache writers avoid one ReaderFile seek/cache lookup per byte.
    virtual bool read_range(uint32_t offset, unsigned char* output, uint32_t count) const;
    virtual bool export_text(TextSink sink, void* context) const;
    virtual uint32_t optimized_size() const { return 0; }
    virtual bool optimized_byte_at(uint32_t, unsigned char&) const { return false; }
    virtual bool cache_archive_layout(uint32_t&, uint32_t&, uint16_t&) const { return false; }
    // Called only after a newly written cache transaction has been verified.
    virtual void cache_persisted() const {}
    // External cache is a checked payload, not a replacement for the ZIP source.
    virtual bool companion_present() const { return false; }
    virtual bool companion_cache(uint32_t&, uint32_t&) const { return false; }
    virtual bool companion_read(uint32_t, unsigned char*, uint32_t) const { return false; }
    // CRC of the ZIP central directory (gbareader's own entries excluded), as
    // verified against the companion when the source was opened.
    virtual bool archive_fingerprint(uint32_t&) const { return false; }
};

class MemorySource final : public ByteSource {
public:
    MemorySource(const unsigned char* data, uint32_t size) : _data(data), _size(size) {}
    uint32_t size() const override { return _size; }
    bool byte_at(uint32_t offset, unsigned char& value) const override;
private:
    const unsigned char* _data;
    uint32_t _size;
};

struct PageLine {
    char text[PAGE_LINE_BYTES];
    bool paragraph_break;
};

struct Page {
    uint32_t start_offset;
    uint32_t next_offset;
    int line_count;
    bool eof;
    PageLine lines[PAGE_MAX_LINES];
};

using GlyphWidth = int (*)(uint32_t codepoint);

struct PageHistory {
    uint32_t offsets[PAGE_HISTORY_MAX];
    int count;
    int head;
    uint32_t lazy_anchor;
    bool lazy;
};

enum class HistoryRebuildState : uint8_t { IDLE, BUILDING, READY, FAILED };

struct PageHistoryRebuild {
    PageHistory rebuilt;
    Page scan;
    uint32_t anchor;
    HistoryRebuildState state;
    bool initialized;
};

bool layout_page(const ByteSource& source, uint32_t offset, const Settings& settings,
                 GlyphWidth glyph_width, Page& page);
bool open_first_page(const ByteSource& source, const Settings& settings, GlyphWidth glyph_width,
                     PageHistory& history, Page& page);
bool open_page_at(const ByteSource& source, uint32_t offset, const Settings& settings,
                  GlyphWidth glyph_width, PageHistory& history, Page& page);
bool next_page(const ByteSource& source, const Settings& settings, GlyphWidth glyph_width,
               PageHistory& history, const Page& current, Page& next);
bool previous_page(const ByteSource& source, const Settings& settings, GlyphWidth glyph_width,
                   PageHistory& history, Page& previous);
// Back history is rebuilt from a line start about this far before its anchor, not from
// the book start, so Back after a deep Go to or resume only lays out a few pages.
constexpr uint32_t HISTORY_REBUILD_WINDOW = 8 * 1024;
// With fewer remembered pages than this, the pages before the oldest one load ahead.
constexpr int HISTORY_PREFETCH_PAGES = 8;
void begin_history_rebuild(uint32_t anchor, PageHistoryRebuild& rebuild);
// Where the next older window must end: the oldest remembered page, else `current`.
uint32_t history_rebuild_anchor(const PageHistory& history, uint32_t current);

// Background page numbering: counts pages from the book start up to `target`,
// one page layout per step, like the Back-history rebuild.
constexpr int PAGE_COUNT_CHECKPOINTS = 32;
struct PageCount {
    Page scan;
    uint32_t target;
    uint32_t pages; // Pages starting before target.
    uint32_t start_offset; // Where the scan resumes; `pages` already counts pages before it.
    HistoryRebuildState state;
    bool initialized;
    // Page starts already counted with these settings (offset, pages before it), every
    // `checkpoint_stride` pages, so a new target resumes instead of starting over.
    uint32_t checkpoint_offsets[PAGE_COUNT_CHECKPOINTS];
    uint32_t checkpoint_pages[PAGE_COUNT_CHECKPOINTS];
    int checkpoint_count;
    uint32_t checkpoint_stride;
};
void begin_page_count(uint32_t target, PageCount& count);
// Same settings, new target (Go to): keep counted pages and resume from the nearest
// counted page start before `target`.
void retarget_page_count(uint32_t target, PageCount& count);
HistoryRebuildState step_page_count(const ByteSource& source, const Settings& settings,
                                    GlyphWidth glyph_width, PageCount& count);
// Pages before the target: exact when READY, projected from the pages counted so far
// while BUILDING, -1 before there is anything to project from (or FAILED).
int page_count_estimate(const PageCount& count);
// 0-100 position of a page; the last page is always 100.
int page_percent(const Page& page, uint32_t source_size);
// Start of the paragraph holding `percent` of the book, or the next word when that
// paragraph starts too far back. Always a valid page start below the source size.
bool percent_offset(const ByteSource& source, int percent, uint32_t& offset);
HistoryRebuildState step_history_rebuild(const ByteSource& source, const Settings& settings,
                                         GlyphWidth glyph_width, PageHistoryRebuild& rebuild);
bool adopt_rebuilt_history(PageHistoryRebuild& rebuild, PageHistory& history);
// Put a ready window that ends at the oldest remembered page in front of it (the newest
// PAGE_HISTORY_MAX pages are kept).
bool prepend_rebuilt_history(PageHistoryRebuild& rebuild, PageHistory& history);

}
