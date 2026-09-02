#pragma once

#include <cstddef>
#include <cstdint>

namespace reader {

constexpr int SCREEN_WIDTH = 240;
constexpr int BODY_SIDE_MARGIN = 8;
constexpr int FONT_HEIGHT = 16;
constexpr int MIN_LINE_SPACING = 1;
constexpr int MAX_LINE_SPACING = 4;
constexpr int MIN_MARGIN = 1;
constexpr int MAX_MARGIN = 4;
constexpr int PAGE_MAX_LINES = 10;
constexpr int PAGE_LINE_BYTES = 256;
constexpr int PAGE_HISTORY_MAX = 64;

struct Settings {
    uint8_t line_spacing;
    uint8_t top_margin;
    uint8_t bottom_margin;
};

enum class SettingField : uint8_t { LINE_SPACING, TOP_MARGIN, BOTTOM_MARGIN };

Settings default_settings();
void clamp_settings(Settings& settings);
void adjust_setting(Settings& settings, SettingField field, int delta);

class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual uint32_t size() const = 0;
    virtual bool byte_at(uint32_t offset, unsigned char& value) const = 0;
    virtual uint32_t optimized_size() const { return 0; }
    virtual bool optimized_byte_at(uint32_t, unsigned char&) const { return false; }
    virtual bool cache_archive_layout(uint32_t&, uint32_t&, uint16_t&) const { return false; }
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
void begin_history_rebuild(uint32_t anchor, PageHistoryRebuild& rebuild);
HistoryRebuildState step_history_rebuild(const ByteSource& source, const Settings& settings,
                                         GlyphWidth glyph_width, PageHistoryRebuild& rebuild);
bool adopt_rebuilt_history(PageHistoryRebuild& rebuild, PageHistory& history);

}
