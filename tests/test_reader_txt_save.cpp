#include "reader_txt_save.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace reader;

namespace {

int mono_width(uint32_t cp) { return cp == ' ' ? 4 : (cp < 0x80 ? 8 : 16); }

const unsigned char legacy_v1_footer[] =
    "\n[GBAR-SAVE:1;O= 0000123456;S=1;T=1;B=1;C=59AAEAA4                                             \n";
static_assert(sizeof(legacy_v1_footer) == TXT_SAVE_FOOTER_V1_SIZE + 1);

void test_v2_round_trip_preserves_full_history_ring()
{
    TxtSaveFooter input{};
    input.byte_offset = 123456;
    input.settings = {2, 3, 4};
    input.history.count = PAGE_HISTORY_MAX;
    input.history.head = 7;
    for(int index = 0; index < PAGE_HISTORY_MAX; ++index) {
        int slot = (input.history.head + index) % PAGE_HISTORY_MAX;
        input.history.offsets[slot] = uint32_t((index + 1) * 100);
    }

    unsigned char bytes[TXT_SAVE_FOOTER_SIZE]{};
    make_txt_save_footer(input, bytes);

    TxtSaveFooter output{};
    assert(looks_like_txt_save_footer(bytes, sizeof(bytes)));
    assert(parse_txt_save_footer(bytes, sizeof(bytes), output));
    assert(output.byte_offset == input.byte_offset);
    assert(output.settings.line_spacing == 2);
    assert(output.settings.top_margin == 3);
    assert(output.settings.bottom_margin == 4);
    assert(output.history.count == PAGE_HISTORY_MAX);
    assert(output.history.head == 0);
    for(int index = 0; index < PAGE_HISTORY_MAX; ++index)
        assert(output.history.offsets[index] == uint32_t((index + 1) * 100));
    assert(! output.history.lazy);
}

void test_v1_footer_remains_readable_and_requests_lazy_history()
{
    TxtSaveFooter output{};
    assert(looks_like_txt_save_footer(legacy_v1_footer, TXT_SAVE_FOOTER_V1_SIZE));
    assert(parse_txt_save_footer(legacy_v1_footer, TXT_SAVE_FOOTER_V1_SIZE, output));
    assert(output.byte_offset == 123456);
    assert(output.settings.line_spacing == 1);
    assert(output.settings.top_margin == 1);
    assert(output.settings.bottom_margin == 1);
    assert(output.history.count == 0);
    assert(output.history.lazy);
    assert(output.history.lazy_anchor == 123456);
}

void test_checksum_rejects_corruption()
{
    TxtSaveFooter input{};
    input.byte_offset = 123456;
    input.settings = {1, 1, 1};
    unsigned char bytes[TXT_SAVE_FOOTER_SIZE]{};
    make_txt_save_footer(input, bytes);
    bytes[100] ^= 1;

    TxtSaveFooter output{};
    assert(looks_like_txt_save_footer(bytes, sizeof(bytes)));
    assert(! parse_txt_save_footer(bytes, sizeof(bytes), output));
}

void test_v3_ascii_footer_restarts_incomplete_rebuild_safely()
{
    unsigned char text[12000];
    for(uint32_t i = 0; i < sizeof(text); ++i) text[i] = (i % 5 == 4) ? '\n' : 'a';
    MemorySource source(text, sizeof(text));
    TxtSaveFooter input{};
    input.byte_offset = 4321;
    input.settings = {2, 3, 4};
    input.history.count = 2;
    input.history.offsets[0] = 100;
    input.history.offsets[1] = 200;
    input.history_rebuild.anchor = 4321;
    input.history_rebuild.scan.start_offset = 300;
    input.history_rebuild.state = HistoryRebuildState::BUILDING;
    input.history_rebuild.initialized = true;
    unsigned char bytes[TXT_SAVE_FOOTER_SIZE]{};
    make_txt_save_footer(input, bytes);
    for(unsigned char byte : bytes) assert(byte == '\n' || (byte >= ' ' && byte <= '~'));
    TxtSaveFooter output{};
    assert(parse_txt_save_footer(bytes, sizeof(bytes), output));
    assert(output.history_rebuild.state == HistoryRebuildState::BUILDING);
    assert(output.history_rebuild.anchor == 4321);
    assert(!output.history_rebuild.initialized);
    while(output.history_rebuild.state == HistoryRebuildState::BUILDING)
        assert(step_history_rebuild(source, output.settings, mono_width, output.history_rebuild) !=
               HistoryRebuildState::FAILED);
    assert(output.history_rebuild.state == HistoryRebuildState::READY);
}

}

int main()
{
    test_v2_round_trip_preserves_full_history_ring();
    test_v1_footer_remains_readable_and_requests_lazy_history();
    test_checksum_rejects_corruption();
    test_v3_ascii_footer_restarts_incomplete_rebuild_safely();
    std::puts("PASS: TXT save footer v3 history");
}
