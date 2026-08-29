#include "reader_txt_save.h"

#include <cstring>

namespace reader {
namespace {

constexpr int V1_OFFSET_AT = 17;
constexpr int V1_SPACING_AT = 30;
constexpr int V1_TOP_AT = 34;
constexpr int V1_BOTTOM_AT = 38;
constexpr int V1_CHECKSUM_AT = 42;

constexpr unsigned char V2_MAGIC[] = "\n[GBAR-SAVE:2]\n";
constexpr int V2_SIZE_AT = 16;
constexpr int V2_OFFSET_AT = 20;
constexpr int V2_SPACING_AT = 24;
constexpr int V2_TOP_AT = 25;
constexpr int V2_BOTTOM_AT = 26;
constexpr int V2_HISTORY_COUNT_AT = 27;
constexpr int V2_CHECKSUM_AT = 28;
constexpr int V2_HISTORY_AT = 32;

uint32_t hash_v1(const unsigned char* bytes)
{
    uint32_t value = 2166136261u;
    for(int index = 0; index < TXT_SAVE_FOOTER_V1_SIZE; ++index) {
        if(index < V1_CHECKSUM_AT || index >= V1_CHECKSUM_AT + 8)
            value = (value ^ bytes[index]) * 16777619u;
    }
    return value;
}

uint32_t hash_v2(const unsigned char* bytes)
{
    uint32_t value = 2166136261u;
    for(int index = 0; index < TXT_SAVE_FOOTER_SIZE; ++index) {
        if(index < V2_CHECKSUM_AT || index >= V2_CHECKSUM_AT + 4)
            value = (value ^ bytes[index]) * 16777619u;
    }
    return value;
}

void write_u32(unsigned char* output, uint32_t value)
{
    output[0] = static_cast<unsigned char>(value);
    output[1] = static_cast<unsigned char>(value >> 8);
    output[2] = static_cast<unsigned char>(value >> 16);
    output[3] = static_cast<unsigned char>(value >> 24);
}

uint32_t read_u32(const unsigned char* input)
{
    return uint32_t(input[0]) |
           (uint32_t(input[1]) << 8) |
           (uint32_t(input[2]) << 16) |
           (uint32_t(input[3]) << 24);
}

bool decimal10_read(const unsigned char* input, uint32_t& value)
{
    value = 0;
    for(int index = 0; index < 10; ++index) {
        if(input[index] < '0' || input[index] > '9') return false;
        uint32_t next = value * 10 + uint32_t(input[index] - '0');
        if(next < value) return false;
        value = next;
    }
    return true;
}

bool hex8_read(const unsigned char* input, uint32_t& value)
{
    value = 0;
    for(int index = 0; index < 8; ++index) {
        unsigned char character = input[index];
        uint32_t digit = character >= '0' && character <= '9' ? character - '0' :
                         character >= 'A' && character <= 'F' ? character - 'A' + 10 : 99;
        if(digit > 15) return false;
        value = (value << 4) | digit;
    }
    return true;
}

bool looks_like_v1(const unsigned char* input)
{
    constexpr unsigned char prefix[] = "\n[GBAR-SAVE:";
    return std::memcmp(input, prefix, sizeof(prefix) - 1) == 0;
}

bool parse_v1(const unsigned char* input, TxtSaveFooter& footer)
{
    if(!looks_like_v1(input) || input[TXT_SAVE_FOOTER_V1_SIZE - 1] != '\n') return false;
    constexpr unsigned char prefix[] = "[GBAR-SAVE:1;O=";
    if(std::memcmp(input + 1, prefix, sizeof(prefix) - 1) != 0) return false;
    if(input[27] != ';' || input[28] != 'S' || input[29] != '=' || input[31] != ';' ||
       input[32] != 'T' || input[33] != '=' || input[35] != ';' || input[36] != 'B' ||
       input[37] != '=' || input[39] != ';' || input[40] != 'C' || input[41] != '=') return false;
    if(input[V1_SPACING_AT] < '1' || input[V1_SPACING_AT] > '4' ||
       input[V1_TOP_AT] < '1' || input[V1_TOP_AT] > '4' ||
       input[V1_BOTTOM_AT] < '1' || input[V1_BOTTOM_AT] > '4') return false;

    uint32_t offset = 0;
    uint32_t checksum = 0;
    if(!decimal10_read(input + V1_OFFSET_AT, offset) ||
       !hex8_read(input + V1_CHECKSUM_AT, checksum) || checksum != hash_v1(input)) return false;

    footer = {};
    footer.byte_offset = offset;
    footer.settings = {uint8_t(input[V1_SPACING_AT] - '0'),
                       uint8_t(input[V1_TOP_AT] - '0'),
                       uint8_t(input[V1_BOTTOM_AT] - '0')};
    footer.history.lazy = offset > 0;
    footer.history.lazy_anchor = offset;
    return true;
}

}

void make_txt_save_footer(const TxtSaveFooter& footer,
                          unsigned char output[TXT_SAVE_FOOTER_SIZE])
{
    std::memset(output, ' ', TXT_SAVE_FOOTER_SIZE);
    std::memcpy(output, V2_MAGIC, sizeof(V2_MAGIC) - 1);
    output[TXT_SAVE_FOOTER_SIZE - 1] = '\n';
    write_u32(output + V2_SIZE_AT, TXT_SAVE_FOOTER_SIZE);
    write_u32(output + V2_OFFSET_AT, footer.byte_offset);
    output[V2_SPACING_AT] = footer.settings.line_spacing;
    output[V2_TOP_AT] = footer.settings.top_margin;
    output[V2_BOTTOM_AT] = footer.settings.bottom_margin;

    int count = footer.history.count;
    if(count < 0) count = 0;
    if(count > PAGE_HISTORY_MAX) count = PAGE_HISTORY_MAX;
    output[V2_HISTORY_COUNT_AT] = static_cast<unsigned char>(count);
    for(int index = 0; index < count; ++index) {
        int source_index = (footer.history.head + index) % PAGE_HISTORY_MAX;
        write_u32(output + V2_HISTORY_AT + index * 4, footer.history.offsets[source_index]);
    }
    write_u32(output + V2_CHECKSUM_AT, hash_v2(output));
}

bool looks_like_txt_save_footer(const unsigned char* input, int size)
{
    if(!input) return false;
    if(size == TXT_SAVE_FOOTER_SIZE)
        return std::memcmp(input, V2_MAGIC, sizeof(V2_MAGIC) - 1) == 0;
    if(size == TXT_SAVE_FOOTER_V1_SIZE) return looks_like_v1(input);
    return false;
}

bool parse_txt_save_footer(const unsigned char* input, int size, TxtSaveFooter& footer)
{
    if(!input) return false;
    if(size == TXT_SAVE_FOOTER_V1_SIZE) return parse_v1(input, footer);
    if(size != TXT_SAVE_FOOTER_SIZE || !looks_like_txt_save_footer(input, size) ||
       input[TXT_SAVE_FOOTER_SIZE - 1] != '\n' ||
       read_u32(input + V2_SIZE_AT) != TXT_SAVE_FOOTER_SIZE ||
       read_u32(input + V2_CHECKSUM_AT) != hash_v2(input)) return false;

    const int count = input[V2_HISTORY_COUNT_AT];
    const uint8_t spacing = input[V2_SPACING_AT];
    const uint8_t top = input[V2_TOP_AT];
    const uint8_t bottom = input[V2_BOTTOM_AT];
    if(count > PAGE_HISTORY_MAX || spacing < MIN_LINE_SPACING || spacing > MAX_LINE_SPACING ||
       top < MIN_MARGIN || top > MAX_MARGIN || bottom < MIN_MARGIN || bottom > MAX_MARGIN)
        return false;

    footer = {};
    footer.byte_offset = read_u32(input + V2_OFFSET_AT);
    footer.settings = {spacing, top, bottom};
    footer.history.count = count;
    uint32_t previous = 0;
    for(int index = 0; index < count; ++index) {
        uint32_t offset = read_u32(input + V2_HISTORY_AT + index * 4);
        if(offset >= footer.byte_offset || (index > 0 && offset <= previous)) return false;
        footer.history.offsets[index] = offset;
        previous = offset;
    }
    footer.history.lazy = count == 0 && footer.byte_offset > 0;
    footer.history.lazy_anchor = footer.byte_offset;
    return true;
}

}
