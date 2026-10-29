#pragma once

#include "reader_core.h"

#include <cstdint>

namespace reader {

constexpr int TXT_SAVE_FOOTER_V1_SIZE = 96;
constexpr int TXT_SAVE_FOOTER_V2_SIZE = 384;
// Fixed ASCII v3: footer state survives partial Back-history reconstruction.
constexpr int TXT_SAVE_FOOTER_SIZE = 800;
constexpr uint8_t CURRENT_DISPLAY_LAYOUT = 4; // Paragraph gap + centered page geometry.
// Same renderer with a turned page: the angle is stored in the old spare field.
// Earlier versions do not know layout 5, so they rebuild such a history instead
// of reusing page breaks made for another page size.
constexpr uint8_t TURNED_DISPLAY_LAYOUT = 5;

struct TxtSaveFooter {
    uint32_t byte_offset;
    Settings settings;
    PageHistory history;
    PageHistoryRebuild history_rebuild;
    // New saves use this renderer; legacy footers parse as layout zero.
    uint8_t display_layout = CURRENT_DISPLAY_LAYOUT;
};

// A save state to fill in, kept off the stack (one at a time).
TxtSaveFooter& footer_scratch();
// Set a save state in place (`state = {...}` would build a 4 KB temporary on the stack).
void clear_footer(TxtSaveFooter& state);
void set_footer(TxtSaveFooter& state, uint32_t byte_offset, const Settings& settings,
                const PageHistory& history, const PageHistoryRebuild& rebuild);

void restore_saved_history(const TxtSaveFooter& footer, PageHistory& history,
                           PageHistoryRebuild& rebuild);
void make_txt_save_footer(const TxtSaveFooter& footer,
                          unsigned char output[TXT_SAVE_FOOTER_SIZE]);
bool parse_txt_save_footer(const unsigned char* input, int size, TxtSaveFooter& footer);
bool looks_like_txt_save_footer(const unsigned char* input, int size);

}
