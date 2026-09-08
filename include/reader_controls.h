#pragma once
#include "reader_ui_state.h"
#include <cstdint>
namespace reader {
constexpr int CONTROLS_LINES = 6;
extern const char* const controls_titles[CONTROLS_PAGE_COUNT];
extern const char* const controls_lines[CONTROLS_PAGE_COUNT][CONTROLS_LINES];
void draw_controls(uint8_t* pixels, int page);
}
