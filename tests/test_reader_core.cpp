#include "reader_core.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace reader;

static int mono_width(uint32_t cp)
{
    return cp == ' ' ? 4 : (cp < 0x80 ? 8 : 16);
}

class FaultSource final : public ByteSource {
public:
    FaultSource(const unsigned char* data, uint32_t size, uint32_t fail_at) :
        _data(data), _size(size), _fail_at(fail_at) {}

    uint32_t size() const override { return _size; }
    bool byte_at(uint32_t offset, unsigned char& value) const override
    {
        if(offset >= _size || offset >= _fail_at) return false;
        value = _data[offset];
        return true;
    }

private:
    const unsigned char* _data;
    uint32_t _size;
    uint32_t _fail_at;
};

class CountingSource final : public ByteSource {
public:
    CountingSource(const unsigned char* data, uint32_t size) : _data(data), _size(size) {}
    uint32_t size() const override { return _size; }
    bool byte_at(uint32_t offset, unsigned char& value) const override
    {
        if(offset >= _size) return false;
        if(offset == 0) ++zero_reads;
        if(offset > max_offset) max_offset = offset;
        ++reads;
        value = _data[offset];
        return true;
    }
    mutable int zero_reads = 0;
    mutable uint32_t max_offset = 0;
    mutable uint32_t reads = 0;
private:
    const unsigned char* _data;
    uint32_t _size;
};

static void test_page_does_not_read_unfittable_line()
{
    const unsigned char text[] = "a\nb\nc\nd\ne\nf\ng\nh\ni\nUNREADABLE";
    FaultSource source(text, sizeof(text) - 1, 19);
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(page.line_count == 9 && page.next_offset == 18 && !page.eof);
}

static void test_bom_crlf_paragraphs_and_wrap()
{
    const unsigned char text[] =
        "\xEF\xBB\xBFOne two three\r\n\r\npneumonoultramicroscopicsilicovolcanoconiosis\nlast";
    MemorySource source(text, sizeof(text) - 1);
    Settings settings = default_settings();
    Page page{};
    assert(layout_page(source, 0, settings, mono_width, page));
    assert(page.start_offset == 3);                 // BOM is not rendered.
    assert(page.line_count >= 3);
    assert(std::strcmp(page.lines[0].text, "One two three") == 0);
    assert(! page.lines[1].paragraph_break);         // repeated newlines are collapsed.
    assert(std::strncmp(page.lines[1].text, "pneumono", 8) == 0);
    assert(std::strcmp(page.lines[1].text, "pneumonoultramicroscopicsilicovolcanoconiosis") != 0);
    assert(page.next_offset > page.start_offset);
}

static void test_truncated_utf8_at_physical_eof()
{
    const char* codepoints[] = {u8"£", u8"€", u8"🙂"};
    for(const char* codepoint : codepoints) {
        for(uint32_t count = 1; count < std::strlen(codepoint); ++count) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(codepoint);
            MemorySource source(bytes, count);
            Page page{}; PageHistory history{};
            assert(open_first_page(source, default_settings(), mono_width, history, page));
            assert(page.start_offset == 0 && page.next_offset == count && page.eof);
            assert(page.line_count == 1 && std::strlen(page.lines[0].text) == count);
            // Canonical replacement bytes keep the OFF native painter UTF-8 safe.
            for(uint32_t i = 0; i < count; ++i) assert(page.lines[0].text[i] == '?');
        }
    }
}

static void test_invalid_utf8_fallback()
{
    const unsigned char text[] = {'A', 0xC0, 0xAF, 'B'};
    MemorySource source(text, sizeof(text));
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(std::strcmp(page.lines[0].text, "A??B") == 0);
}

static void test_arabic_logical_bytes_preserved()
{
    const unsigned char text[] = {'A', 0xD8, 0xA7, 'B'}; // U+0627 ARABIC LETTER ALEF
    MemorySource source(text, sizeof(text));
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(std::strcmp(page.lines[0].text, u8"AاB") == 0);
}

static void test_readable_fallbacks_for_unsupported_equivalents()
{
    const unsigned char text[] = {
        'A', 0xE2, 0x88, 0x92, 'B', ' ', // U+2212 MINUS SIGN
        0xEF, 0xBC, 0x82, 'q', 0xEF, 0xBC, 0x82 // U+FF02 FULLWIDTH QUOTATION MARK
    };
    MemorySource source(text, sizeof(text));
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(std::strcmp(page.lines[0].text, "A-B \"q\"") == 0);
}

static void test_settings_bounds_and_pagination_checkpoints()
{
    static_assert(MIN_LINE_SPACING == 0 && MAX_LINE_SPACING == 4);
    Settings defaults = default_settings();
    assert(defaults.line_spacing == 1);
    assert(defaults.paragraph_gap == ParagraphGap::FULL);
    Settings invalid = { 255, ParagraphGap(200) };
    clamp_settings(invalid);
    assert(invalid.line_spacing == MAX_LINE_SPACING);
    assert(invalid.paragraph_gap == ParagraphGap::FULL);

    Settings s = default_settings();
    for(int i = 0; i < 100; ++i) adjust_setting(s, SettingField::LINE_SPACING, -1);
    assert(s.line_spacing == MIN_LINE_SPACING);
    for(int i = 0; i < 100; ++i) adjust_setting(s, SettingField::PARAGRAPH_GAP, -1);
    assert(s.paragraph_gap == ParagraphGap::NONE);
    for(int i = 0; i < 100; ++i) adjust_setting(s, SettingField::PARAGRAPH_GAP, 1);
    assert(s.paragraph_gap == ParagraphGap::FULL);

    // Lines per page and the centered first row for every spacing.
    const int expected_lines[] = {10, 9, 9, 8, 8};
    const int expected_top[] = {0, 4, 0, 5, 2};
    for(int spacing = MIN_LINE_SPACING; spacing <= MAX_LINE_SPACING; ++spacing) {
        Settings layout = default_settings();
        layout.line_spacing = uint8_t(spacing);
        assert(lines_per_page(layout) == expected_lines[spacing]);
        assert(page_top(layout) == expected_top[spacing]);
    }
    // The default full gap is exactly one empty line, as before.
    assert(paragraph_gap_pixels(default_settings()) == FONT_HEIGHT + 1);

    const char text[] = "one two three four five six seven eight nine ten eleven twelve "
                        "thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty "
                        "twentyone twentytwo twentythree twentyfour twentyfive twentysix "
                        "twentyseven twentyeight twentynine thirty thirtyone thirtytwo "
                        "thirtythree thirtyfour thirtyfive thirtysix thirtyseven thirtyeight";
    MemorySource source(reinterpret_cast<const unsigned char*>(text), sizeof(text) - 1);
    PageHistory history{};
    Page first{}, second{}, back{};
    assert(open_first_page(source, s, mono_width, history, first));
    assert(next_page(source, s, mono_width, history, first, second));
    assert(second.start_offset == first.next_offset);
    PageHistory saved_history = history;
    PageHistory resumed_history{};
    Page resumed{}, resumed_back{};
    assert(open_page_at(source, second.start_offset, s, mono_width, resumed_history, resumed));
    resumed_history = saved_history; // Version-2 footer restores prior page checkpoints.
    assert(resumed.start_offset == second.start_offset);
    assert(previous_page(source, s, mono_width, resumed_history, resumed_back));
    assert(resumed_back.start_offset == first.start_offset);

    assert(previous_page(source, s, mono_width, history, back));
    assert(back.start_offset == first.start_offset);
}

static void test_excess_whitespace_is_collapsed()
{
    const unsigned char text[] = "One  two   three      four\n\n\nFive\r\n\r\nSix";
    MemorySource source(text, sizeof(text) - 1);
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(page.line_count == 3);
    assert(std::strcmp(page.lines[0].text, "One  two three four") == 0);
    assert(std::strcmp(page.lines[1].text, "Five") == 0);
    assert(std::strcmp(page.lines[2].text, "Six") == 0);
    assert(page.lines[0].paragraph_break);
    assert(page.lines[1].paragraph_break);
    assert(! page.lines[2].paragraph_break);
}

static void test_source_read_failures_are_reported()
{
    const unsigned char text[] = "readable then unavailable";
    FaultSource unavailable(text, sizeof(text) - 1, 0);
    Page page{};
    assert(! layout_page(unavailable, 0, default_settings(), mono_width, page));
    assert(page.line_count == 0);
    assert(page.next_offset == 0);
    assert(! page.eof);

    FaultSource short_source(text, sizeof(text) - 1, 8);
    assert(! layout_page(short_source, 0, default_settings(), mono_width, page));
    assert(page.next_offset <= 8);

    const unsigned char bom[] = {0xEF, 0xBB, 0xBF, 'x'};
    FaultSource broken_bom(bom, sizeof(bom), 1);
    assert(! layout_page(broken_bom, 0, default_settings(), mono_width, page));

    const unsigned char long_text[] =
        "one two three four five six seven eight nine ten eleven twelve thirteen fourteen "
        "fifteen sixteen seventeen eighteen nineteen twenty twentyone twentytwo twentythree "
        "twentyfour twentyfive twentysix twentyseven twentyeight twentynine thirty ";
    MemorySource complete(long_text, sizeof(long_text) - 1);
    Settings tight = { MAX_LINE_SPACING, ParagraphGap::FULL };
    PageHistory history{};
    Page first{}, unchanged{};
    assert(open_first_page(complete, tight, mono_width, history, first));
    assert(! first.eof);

    FaultSource fail_next(long_text, sizeof(long_text) - 1, first.next_offset);
    assert(! next_page(fail_next, tight, mono_width, history, first, unchanged));
    assert(history.count == 0); // A failed page read must not add a back checkpoint.

    history.offsets[0] = 0;
    history.count = 1;
    FaultSource fail_previous(long_text, sizeof(long_text) - 1, 0);
    assert(! previous_page(fail_previous, tight, mono_width, history, unchanged));
    assert(history.count == 1); // A failed page read must not consume a checkpoint.
}

static void test_bookmark_restore_rebuilds_history_incrementally()
{
    unsigned char text[24000];
    for(uint32_t i = 0; i < sizeof(text); ++i) text[i] = (i % 5 == 4) ? '\n' : 'a';
    CountingSource source(text, sizeof(text));
    PageHistory history{};
    Page page{};
    assert(open_page_at(source, 1000, default_settings(), mono_width, history, page));
    assert(page.start_offset == 1000);
    assert(source.zero_reads == 0); // Exact bookmark restore does not scan from byte zero.

    Page before{};
    const uint32_t reads_before_back = source.reads;
    assert(!previous_page(source, default_settings(), mono_width, history, before));
    assert(source.reads == reads_before_back); // Back never starts a blocking full-book scan.

    source.zero_reads = 0;
    source.max_offset = 0;
    PageHistoryRebuild rebuild{};
    begin_history_rebuild(1000, rebuild);
    assert(source.zero_reads == 0); // Starting the job performs no I/O.
    assert(step_history_rebuild(source, default_settings(), mono_width, rebuild) ==
           HistoryRebuildState::BUILDING);
    assert(source.zero_reads > 0);
    assert(source.max_offset < 1000); // One step is bounded to one laid-out page.

    int steps = 1;
    while(rebuild.state == HistoryRebuildState::BUILDING && steps < 1000) {
        step_history_rebuild(source, default_settings(), mono_width, rebuild);
        ++steps;
    }
    assert(rebuild.state == HistoryRebuildState::READY);
    assert(adopt_rebuilt_history(rebuild, history));
    assert(previous_page(source, default_settings(), mono_width, history, before));
    assert(before.start_offset < 1000);
}

static void test_page_history_is_circular()
{
    unsigned char text[24000];
    for(uint32_t i = 0; i < sizeof(text); ++i) text[i] = (i % 5 == 4) ? '\n' : 'a';
    MemorySource source(text, sizeof(text));
    PageHistory history{};
    Page page{};
    assert(open_first_page(source, default_settings(), mono_width, history, page));
    uint32_t remembered[PAGE_HISTORY_MAX + 8]{};
    for(int i = 0; i < PAGE_HISTORY_MAX + 8; ++i) {
        remembered[i] = page.start_offset;
        Page next{};
        assert(next_page(source, default_settings(), mono_width, history, page, next));
        page = next;
    }
    assert(history.count == PAGE_HISTORY_MAX);
    assert(history.head > 0);
    for(int i = PAGE_HISTORY_MAX + 7; i >= 8; --i) {
        Page previous{};
        assert(previous_page(source, default_settings(), mono_width, history, previous));
        assert(previous.start_offset == remembered[i]);
    }
    assert(history.count == 0);
}

static void test_display_punctuation_preserves_source_offsets()
{
    const unsigned char text[] = u8"‘’‚“”„‐‑ …–—«»•£€";
    MemorySource source(text, sizeof(text) - 1);
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(std::strcmp(page.lines[0].text, u8"'''\"\"\"-- …–—«»•£€") == 0);
    assert(page.next_offset == sizeof(text) - 1 && page.eof);
    unsigned char original[sizeof(text) - 1];
    assert(source.read_range(0, original, sizeof(original)));
    assert(std::memcmp(original, text, sizeof(original)) == 0);
}

static void test_page_count_matches_sequential_pages()
{
    unsigned char text[12000];
    for(uint32_t i = 0; i < sizeof(text); ++i) text[i] = (i % 37 == 36) ? '\n' : (i % 6 == 5 ? ' ' : 'a');
    MemorySource source(text, sizeof(text));
    const Settings settings = default_settings();
    PageHistory history{};
    Page page{}, next{};
    assert(open_first_page(source, settings, mono_width, history, page));
    PageCount count{};
    begin_page_count(page.start_offset, count);
    assert(count.state == HistoryRebuildState::READY && count.pages == 0);
    for(uint32_t number = 1; !page.eof; ++number) {
        begin_page_count(page.start_offset, count);
        while(count.state == HistoryRebuildState::BUILDING)
            step_page_count(source, settings, mono_width, count);
        assert(count.state == HistoryRebuildState::READY);
        assert(count.pages + 1 == number);
        if(!next_page(source, settings, mono_width, history, page, next)) break;
        page = next;
    }
    assert(page.eof && page_percent(page, source.size()) == 100);
}

static void test_percent_offset_snaps_to_paragraph_or_word()
{
    const char text[] = "first para\nsecond paragraph here\r\nthird\n\n";
    MemorySource source(reinterpret_cast<const unsigned char*>(text), sizeof(text) - 1);
    uint32_t offset = 99;
    assert(percent_offset(source, 0, offset) && offset == 0);
    assert(percent_offset(source, 5, offset) && offset == 0);
    assert(percent_offset(source, 40, offset) && offset == 11); // inside "second"
    assert(percent_offset(source, 100, offset) && offset == 34); // "third", not the trailing breaks
    for(int percent = 0; percent <= 100; ++percent) {
        assert(percent_offset(source, percent, offset));
        assert(offset < source.size() && (offset == 0 || text[offset - 1] == '\n'));
    }

    // One huge paragraph: land on the next word, never inside a UTF-8 sequence.
    static unsigned char long_text[6000];
    for(uint32_t i = 0; i < sizeof(long_text); i += 3) {
        long_text[i] = 0xC3; long_text[i + 1] = 0xA9; long_text[i + 2] = (i % 30 == 27) ? ' ' : 'e';
    }
    MemorySource long_source(long_text, sizeof(long_text));
    for(int percent = 1; percent <= 100; ++percent) {
        assert(percent_offset(long_source, percent, offset));
        assert(offset < long_source.size() && (long_text[offset] & 0xC0) != 0x80);
    }
    MemorySource empty(long_text, 0);
    assert(!percent_offset(empty, 50, offset));
}

int main()
{
    test_page_count_matches_sequential_pages();
    test_percent_offset_snaps_to_paragraph_or_word();
    test_display_punctuation_preserves_source_offsets();
    test_page_does_not_read_unfittable_line();
    Settings original = default_settings(), changed = original;
    assert(same_settings(original, changed));
    const SettingField fields[] = {SettingField::LINE_SPACING, SettingField::PARAGRAPH_GAP};
    for(SettingField field : fields) {
        adjust_setting(changed, field, -1);
        assert(!same_settings(original, changed));
        adjust_setting(changed, field, 1);
        assert(same_settings(original, changed));
    }
    test_bom_crlf_paragraphs_and_wrap();
    test_truncated_utf8_at_physical_eof();
    test_invalid_utf8_fallback();
    test_arabic_logical_bytes_preserved();
    test_readable_fallbacks_for_unsupported_equivalents();
    test_settings_bounds_and_pagination_checkpoints();
    test_excess_whitespace_is_collapsed();
    test_source_read_failures_are_reported();
    test_bookmark_restore_rebuilds_history_incrementally();
    test_page_history_is_circular();
    std::puts("PASS: reader core");
}
