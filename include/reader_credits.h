#pragma once
#include <cstdint>
#include "reader_ui_state.h"
namespace reader {
constexpr int CREDITS_LINES = 6;
extern const char* const credits_titles[CREDITS_PAGE_COUNT];
extern const char* const credits_lines[CREDITS_PAGE_COUNT][CREDITS_LINES];
// Personal page retains its original position and typography.
void draw_credits(uint8_t* pixels, int page = 0);
}
