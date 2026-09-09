#pragma once

#include "reader_core.h"

#include <cstdint>

namespace reader {

constexpr int TXT_SAVE_FOOTER_V1_SIZE = 96;
constexpr int TXT_SAVE_FOOTER_V2_SIZE = 384;
// Fixed ASCII v3: footer state survives partial Back-history reconstruction.
constexpr int TXT_SAVE_FOOTER_SIZE = 800;
constexpr uint8_t CURRENT_DISPLAY_LAYOUT = 2; // Ghoulam shaping/RTL advances.

struct TxtSaveFooter {
    uint32_t byte_offset;
    Settings settings;
    PageHistory history;
    PageHistoryRebuild history_rebuild;
    // New saves use this renderer; legacy footers parse as layout zero.
    uint8_t display_layout = CURRENT_DISPLAY_LAYOUT;
};

void restore_saved_history(const TxtSaveFooter& footer, PageHistory& history,
                           PageHistoryRebuild& rebuild);
void make_txt_save_footer(const TxtSaveFooter& footer,
                          unsigned char output[TXT_SAVE_FOOTER_SIZE]);
bool parse_txt_save_footer(const unsigned char* input, int size, TxtSaveFooter& footer);
bool looks_like_txt_save_footer(const unsigned char* input, int size);

}
