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

static void test_bom_crlf_paragraphs_and_wrap()
{
    const unsigned char text[] =
        "\xEF\xBB\xBFOne two three\r\n\r\npneumonoultramicroscopicsilicovolcanoconiosis\nlast";
    MemorySource source(text, sizeof(text) - 1);
    Settings settings = default_settings();
    Page page{};
    assert(layout_page(source, 0, settings, mono_width, page));
    assert(page.start_offset == 3);                 // BOM is not rendered.
    assert(page.line_count >= 4);
    assert(std::strcmp(page.lines[0].text, "One two three") == 0);
    assert(page.lines[1].paragraph_break);          // empty physical line preserved.
    assert(std::strcmp(page.lines[2].text, "pneumonoultramicroscopicsilicovolcanoconiosis") != 0);
    assert(page.next_offset > page.start_offset);
}

static void test_invalid_utf8_fallback()
{
    const unsigned char text[] = {'A', 0xC0, 0xAF, 'B'};
    MemorySource source(text, sizeof(text));
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(std::strcmp(page.lines[0].text, "A??B") == 0);
}

static void test_arabic_is_not_supported()
{
    const unsigned char text[] = {'A', 0xD8, 0xA7, 'B'}; // U+0627 ARABIC LETTER ALEF
    MemorySource source(text, sizeof(text));
    Page page{};
    assert(layout_page(source, 0, default_settings(), mono_width, page));
    assert(std::strcmp(page.lines[0].text, "A?B") == 0);
}

static void test_settings_bounds_and_pagination_checkpoints()
{
    Settings s = default_settings();
    for(int i = 0; i < 100; ++i) adjust_setting(s, SettingField::LINE_SPACING, -1);
    assert(s.line_spacing == MIN_LINE_SPACING);
    for(int i = 0; i < 100; ++i) adjust_setting(s, SettingField::TOP_MARGIN, 1);
    assert(s.top_margin == MAX_MARGIN);
    for(int i = 0; i < 100; ++i) adjust_setting(s, SettingField::BOTTOM_MARGIN, 1);
    assert(s.bottom_margin == MAX_MARGIN);

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
    PageHistory resumed_history{};
    Page resumed{}, resumed_back{};
    assert(open_page_at(source, second.start_offset, s, mono_width, resumed_history, resumed));
    assert(resumed.start_offset == second.start_offset);
    assert(previous_page(source, s, mono_width, resumed_history, resumed_back));
    assert(resumed_back.start_offset == first.start_offset);

    assert(previous_page(source, s, mono_width, history, back));
    assert(back.start_offset == first.start_offset);
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
    Settings tight = { MAX_LINE_SPACING, MAX_MARGIN, MAX_MARGIN };
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

int main()
{
    test_bom_crlf_paragraphs_and_wrap();
    test_invalid_utf8_fallback();
    test_arabic_is_not_supported();
    test_settings_bounds_and_pagination_checkpoints();
    test_source_read_failures_are_reported();
    std::puts("PASS: reader core");
}
