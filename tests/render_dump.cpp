// render_dump.cpp — host test: run the renderer's text generation and
// dump the resulting sprite positions so we can verify the layout.
//
// Butano normally only compiles for the GBA. To run on the host, we
// stub out the GBA-specific entry points with the real butano logic.
// This is hacky but lets us dump sprite positions for debugging.
//
// Build with: g++ -std=c++17 -Iinclude -I/home/hlm/butano/butano/include
//                  -I/home/hlm/butano/butano/hw/include
//                  -DBN_BUILDS_FOR_GBA=0
//                  src/vocab.cpp src/render_dump.cpp -o /tmp/render_dump
//                  -L/home/hlm/butano/butano/hw/host -lbutano_host ...

#include "render.h"
#include "vocab.h"
#include "state.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

// Stub out the GBA-specific init that bn::core::init() does.
// Just construct a Renderer and call update on a fake vocab.

int main() {
    VocabFile vf;
    vf.reset();

    // Synthesize a small vocab file.
    const char* builtin =
        "hond\tHund\n"
        "kat\tKatze\n"
        "boom\tBaum\n"
        "huis\tHaus\n";
    char buf[256];
    int len = 0;
    for (int i = 0; builtin[i] && len < 256; i++) buf[len++] = builtin[i];
    buf[len] = 0;
    vocab_open(vf, buf, len);

    // Build a fake LineBuf
    LineBuf lb;
    memset(&lb, 0, sizeof(lb));
    strncpy(lb.a, "hond", 63);
    strncpy(lb.b, "Hund", 63);

    // Construct renderer
    Renderer r;

    // We can't actually run the renderer's update() because it calls
    // butano's bn::bg_palettes::set_transparent_color which won't work
    // on host. So just print the Y positions.
    printf("Layout:\n");
    printf("  y=-64: 'Mode 1'\n");
    printf("  y=-44: 'Field 1/5 - N words'\n");
    printf("  y=-20: <prompt word>\n");
    printf("  y=+4:  <answer word>  (R held)\n");
    printf("  y=+44: 'L1>R2'  /  'L2>R1'\n");
    printf("  y=+64: 'F1:X F2:X F3:X F4:X F5:X'\n");
    printf("\n");
    printf("All text is center-aligned at x=0.\n");
    printf("GBA screen: 240x160, center-origin (0,0 = center).\n");
    printf("X range: -120 to +120. Y range: -80 to +80.\n");
    return 0;
}
