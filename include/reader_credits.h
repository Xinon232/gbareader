#pragma once
#include <cstdint>

namespace reader {
// Draw into the caller's cleared 240x160 indexed bitmap; no sprite allocations.
void draw_credits(uint8_t* pixels);
}
