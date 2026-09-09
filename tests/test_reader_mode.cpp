#include "reader_core.h"
#include <cassert>
#include <cstring>
#include <cstdio>

static void canonical_malformed_output()
{
    const unsigned char bytes[] = {0xE2, 0x82, 'x', 0xF0, 0x80, 0x80, 0x80, 0xED, 0xA0, 0x80, 0xC2};
    reader::MemorySource source(bytes, sizeof(bytes));
    reader::Page page{};
    assert(reader::layout_page(source, 0, reader::default_settings(), nullptr, page));
    assert(!std::strcmp(page.lines[0].text, "??x????????"));
    assert(page.next_offset == sizeof(bytes));
}
int main()
{
    canonical_malformed_output();
    const char* joined = u8"لالالالالالالالالالالالالالالالالالالالالا";
    reader::MemorySource source(reinterpret_cast<const unsigned char*>(joined), std::strlen(joined));
    reader::Page page{};
    assert(reader::layout_page(source, 0, reader::default_settings(), nullptr, page));
    assert(page.line_count > 1); // New sessions default OFF, native fallback widths.
    assert(!reader::default_settings().arabic_shaping);
    auto enabled = reader::default_settings();
    enabled.arabic_shaping = true;
    assert(!reader::same_settings(enabled, reader::default_settings()));
    assert(reader::layout_page(source, 0, enabled, nullptr, page));
    assert(page.line_count == 1);
    puts("PASS: reader mode/canonical bytes");
}
