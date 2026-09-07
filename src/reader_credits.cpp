#include "reader_credits.h"
extern "C" {
#include "font_render.h"
}

namespace reader {
void draw_credits(uint8_t* pixels)
{
    const char* lines[] = {
        "Made by Halim Jarrar",
        "(C) 2026",
        "halim-jarrar.de",
        "monday@halim-jarrar.de"
    };
    for(int i = 0; i < 4; ++i) {
        const unsigned width = font_width(lines[i]);
        draw_text_idx8_bus16_range(lines[i], pixels + (40 + i * 22) * 240 + (240 - width) / 2,
                                  0, width, 240, 1);
    }
}
}
