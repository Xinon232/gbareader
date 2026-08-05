#pragma once
#include "reader_core.h"
#include <cstdint>
namespace reader {
constexpr int TXT_SAVE_FOOTER_SIZE = 96;
struct TxtSaveFooter { uint32_t byte_offset; Settings settings; };
void make_txt_save_footer(const TxtSaveFooter& footer, unsigned char output[TXT_SAVE_FOOTER_SIZE]);
bool parse_txt_save_footer(const unsigned char input[TXT_SAVE_FOOTER_SIZE], TxtSaveFooter& footer);
bool looks_like_txt_save_footer(const unsigned char input[TXT_SAVE_FOOTER_SIZE]);
}
