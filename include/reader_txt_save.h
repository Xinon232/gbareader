#pragma once

#include "reader_core.h"

#include <cstdint>

namespace reader {

constexpr int TXT_SAVE_FOOTER_V1_SIZE = 96;
constexpr int TXT_SAVE_FOOTER_V2_SIZE = 384;
// Fixed ASCII v3: footer state survives partial Back-history reconstruction.
constexpr int TXT_SAVE_FOOTER_SIZE = 800;

struct TxtSaveFooter {
    uint32_t byte_offset;
    Settings settings;
    PageHistory history;
    PageHistoryRebuild history_rebuild;
};

void make_txt_save_footer(const TxtSaveFooter& footer,
                          unsigned char output[TXT_SAVE_FOOTER_SIZE]);
bool parse_txt_save_footer(const unsigned char* input, int size, TxtSaveFooter& footer);
bool looks_like_txt_save_footer(const unsigned char* input, int size);

}
