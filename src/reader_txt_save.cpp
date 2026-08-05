#include "reader_txt_save.h"

namespace reader {
namespace {
constexpr int OFFSET_AT = 17;
constexpr int SPACING_AT = 30;
constexpr int TOP_AT = 34;
constexpr int BOTTOM_AT = 38;
constexpr int CHECKSUM_AT = 42;

uint32_t hash(const unsigned char* bytes)
{
    uint32_t value = 2166136261u;
    for(int i = 0; i < TXT_SAVE_FOOTER_SIZE; ++i) {
        if(i < CHECKSUM_AT || i >= CHECKSUM_AT + 8) value = (value ^ bytes[i]) * 16777619u;
    }
    return value;
}

void decimal10(unsigned char* output, uint32_t value)
{
    for(int i = 9; i >= 0; --i) { output[i] = unsigned('0' + value % 10); value /= 10; }
}

bool decimal10_read(const unsigned char* input, uint32_t& value)
{
    value = 0;
    for(int i = 0; i < 10; ++i) {
        if(input[i] < '0' || input[i] > '9') return false;
        uint32_t next = value * 10 + uint32_t(input[i] - '0');
        if(next < value) return false;
        value = next;
    }
    return true;
}

void hex8(unsigned char* output, uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    for(int i = 7; i >= 0; --i) { output[i] = digits[value & 15]; value >>= 4; }
}

bool hex8_read(const unsigned char* input, uint32_t& value)
{
    value = 0;
    for(int i = 0; i < 8; ++i) {
        unsigned char c = input[i];
        uint32_t digit = (c >= '0' && c <= '9') ? c - '0' :
                         (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 99;
        if(digit > 15) return false;
        value = (value << 4) | digit;
    }
    return true;
}
}

void make_txt_save_footer(const TxtSaveFooter& footer, unsigned char output[TXT_SAVE_FOOTER_SIZE])
{
    for(int i = 0; i < TXT_SAVE_FOOTER_SIZE; ++i) output[i] = ' ';
    output[0] = '\n';
    output[95] = '\n';
    const char prefix[] = "[GBAR-SAVE:1;O=";
    for(int i = 0; prefix[i]; ++i) output[1 + i] = prefix[i];
    decimal10(output + OFFSET_AT, footer.byte_offset);
    output[27] = ';'; output[28] = 'S'; output[29] = '='; output[SPACING_AT] = unsigned('0' + footer.settings.line_spacing);
    output[31] = ';'; output[32] = 'T'; output[33] = '='; output[TOP_AT] = unsigned('0' + footer.settings.top_margin);
    output[35] = ';'; output[36] = 'B'; output[37] = '='; output[BOTTOM_AT] = unsigned('0' + footer.settings.bottom_margin);
    output[39] = ';'; output[40] = 'C'; output[41] = '=';
    hex8(output + CHECKSUM_AT, hash(output));
}

bool looks_like_txt_save_footer(const unsigned char input[TXT_SAVE_FOOTER_SIZE])
{
    const char prefix[] = "\n[GBAR-SAVE:";
    for(int i = 0; prefix[i]; ++i) if(input[i] != unsigned(prefix[i])) return false;
    return true;
}

bool parse_txt_save_footer(const unsigned char input[TXT_SAVE_FOOTER_SIZE], TxtSaveFooter& footer)
{
    if(! looks_like_txt_save_footer(input) || input[95] != '\n') return false;
    const char prefix[] = "[GBAR-SAVE:1;O=";
    for(int i = 0; prefix[i]; ++i) if(input[1 + i] != unsigned(prefix[i])) return false;
    if(input[27] != ';' || input[28] != 'S' || input[29] != '=' || input[31] != ';' ||
       input[32] != 'T' || input[33] != '=' || input[35] != ';' || input[36] != 'B' ||
       input[37] != '=' || input[39] != ';' || input[40] != 'C' || input[41] != '=') return false;
    if(input[SPACING_AT] < '1' || input[SPACING_AT] > '4' || input[TOP_AT] < '1' ||
       input[TOP_AT] > '4' || input[BOTTOM_AT] < '1' || input[BOTTOM_AT] > '4') return false;
    uint32_t offset = 0;
    uint32_t check = 0;
    if(! decimal10_read(input + OFFSET_AT, offset) || ! hex8_read(input + CHECKSUM_AT, check) || check != hash(input)) return false;
    footer.byte_offset = offset;
    footer.settings = { uint8_t(input[SPACING_AT] - '0'), uint8_t(input[TOP_AT] - '0'), uint8_t(input[BOTTOM_AT] - '0') };
    return true;
}
}
