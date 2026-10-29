// Turned rectangles straight on the screen (IWRAM, ARM code: comics move
// the screen every frame while sliding).
#include "reader_turn.h"

namespace reader {
namespace {

constexpr int W = 240, H = 160, LINE = W / 2;

// The screen rectangle showing canvas rectangle (cx, cy, w, h), and how the
// canvas index moves for one screen pixel right (step) and one row down (row_step).
struct Mapping {
    int sx, sy, sw, sh;
    int step_x, step_y;     // canvas (x, y) change per screen pixel right
    int row_x, row_y;       // and per screen row down
    int ax, ay;             // canvas pixel at the rectangle's top left screen pixel
};

Mapping map_rect(Angle angle, int cx, int cy, int w, int h)
{
    Mapping m;
    switch(angle) {
    case Angle::RIGHT:
        m.sx = cy; m.sw = h; m.sy = H - cx - w; m.sh = w;
        m.step_x = 0; m.step_y = 1; m.row_x = -1; m.row_y = 0;
        break;
    case Angle::LEFT:
        m.sx = W - cy - h; m.sw = h; m.sy = cx; m.sh = w;
        m.step_x = 0; m.step_y = -1; m.row_x = 1; m.row_y = 0;
        break;
    case Angle::UPSIDE_DOWN:
        m.sx = W - cx - w; m.sw = w; m.sy = H - cy - h; m.sh = h;
        m.step_x = -1; m.step_y = 0; m.row_x = 0; m.row_y = -1;
        break;
    default:
        m.sx = cx; m.sw = w; m.sy = cy; m.sh = h;
        m.step_x = 1; m.step_y = 0; m.row_x = 0; m.row_y = 1;
        break;
    }
    turned_source(angle, m.sx, m.sy, m.ax, m.ay);
    return m;
}

#if defined(__arm__) && defined(__DEVKITARM__)
// DMA 3 copies (the CPU waits): VRAM to VRAM at the bus's own speed.
void dma_copy(const void* from, void* to, unsigned units, bool words)
{
    volatile uint32_t* dma3 = reinterpret_cast<volatile uint32_t*>(0x040000D4);
    dma3[0] = reinterpret_cast<uintptr_t>(from);
    dma3[1] = reinterpret_cast<uintptr_t>(to);
    dma3[2] = units | (words ? 0x84000000u : 0x80000000u);
}
#endif

// One pixel of a pair: a 16-bit store. (The compiler may turn a plain
// read-modify-write of one byte into an 8-bit store, which video memory
// writes into both pixels of the pair.)
inline void set_low(uint16_t* pair, uint8_t v)
{
    volatile uint16_t* p = pair;
    *p = uint16_t((*p & 0xFF00) | v);
}
inline void set_high(uint16_t* pair, uint8_t v)
{
    volatile uint16_t* p = pair;
    *p = uint16_t((*p & 0xFF) | (v << 8));
}

bool clip(int& cx, int& cy, int& w, int& h, int cw, int ch, int& skip_x, int& skip_y)
{
    skip_x = cx < 0 ? -cx : 0;
    skip_y = cy < 0 ? -cy : 0;
    cx += skip_x; w -= skip_x;
    cy += skip_y; h -= skip_y;
    if(cx + w > cw) w = cw - cx;
    if(cy + h > ch) h = ch - cy;
    return w > 0 && h > 0;
}

}

void turn_copy(const uint8_t* src, int stride, int cx, int cy, int w, int h, uint16_t* screen, Angle angle)
{
    int skip_x, skip_y;
    if(!clip(cx, cy, w, h, turned_width(angle), turned_height(angle), skip_x, skip_y)) return;
    src += skip_y * stride + skip_x;
    const Mapping m = map_rect(angle, cx, cy, w, h);
    const int step = m.step_x + m.step_y * stride, row_step = m.row_x + m.row_y * stride;
    const uint8_t* row = src + (m.ay - cy) * stride + (m.ax - cx);
    for(int y = m.sy; y < m.sy + m.sh; ++y, row += row_step) {
        uint16_t* line = screen + y * LINE;
        const uint8_t* p = row;
        int x = m.sx, n = m.sw;
        if(x & 1) {
            set_high(line + (x >> 1), *p);
            p += step; ++x; --n;
        }
        uint16_t* out = line + (x >> 1);
        for(; n >= 2; n -= 2, p += 2 * step) *out++ = uint16_t(p[0] | (p[step] << 8));
        if(n) set_low(out, *p);
    }
}

void turn_fill(uint8_t color, int cx, int cy, int w, int h, uint16_t* screen, Angle angle)
{
    int skip_x, skip_y;
    if(!clip(cx, cy, w, h, turned_width(angle), turned_height(angle), skip_x, skip_y)) return;
    const Mapping m = map_rect(angle, cx, cy, w, h);
    const uint16_t pair = uint16_t(color | (color << 8));
    for(int y = m.sy; y < m.sy + m.sh; ++y) {
        uint16_t* line = screen + y * LINE;
        int x = m.sx, n = m.sw;
        if(x & 1) { set_high(line + (x >> 1), color); ++x; --n; }
        uint16_t* out = line + (x >> 1);
        for(; n >= 2; n -= 2) *out++ = pair;
        if(n) set_low(out, color);
    }
}

void turn_read(const uint16_t* screen, Angle angle, int cx, int cy, int w, int h, uint8_t* out, int stride)
{
    int skip_x, skip_y;
    if(!clip(cx, cy, w, h, turned_width(angle), turned_height(angle), skip_x, skip_y)) return;
    out += skip_y * stride + skip_x;
    const Mapping m = map_rect(angle, cx, cy, w, h);
    const int step = m.step_x + m.step_y * stride, row_step = m.row_x + m.row_y * stride;
    uint8_t* row = out + (m.ay - cy) * stride + (m.ax - cx);
    for(int y = m.sy; y < m.sy + m.sh; ++y, row += row_step) {
        const uint16_t* line = screen + y * LINE;
        uint8_t* p = row;
        for(int x = m.sx; x < m.sx + m.sw; ++x, p += step) {
            const uint16_t v = line[x >> 1];
            *p = uint8_t(x & 1 ? v >> 8 : v);
        }
    }
}

void turn_shift(const uint16_t* from, uint16_t* to, Angle angle, int dx, int dy)
{
    // The same move on the screen: to(x, y) = from(x + ex, y + ey).
    int ex, ey;
    switch(angle) {
    case Angle::RIGHT: ex = dy; ey = -dx; break;
    case Angle::LEFT: ex = -dy; ey = dx; break;
    case Angle::UPSIDE_DOWN: ex = -dx; ey = -dy; break;
    default: ex = dx; ey = dy; break;
    }
    if(ex <= -W || ex >= W || ey <= -H || ey >= H) return;
    const int y0 = ey < 0 ? -ey : 0, y1 = ey > 0 ? H - ey : H;
    // Whole 16-bit pairs of the destination that have a source.
    const int x0 = ex < 0 ? (-ex + 1) & ~1 : 0, x1 = ex > 0 ? (W - ex) & ~1 : W;
    // (An odd move leaves one pixel beside the pairs, even with no pairs.)
    const int pairs = x1 > x0 ? (x1 - x0) / 2 : 0;
    if(!pairs && !(ex & 1)) return;
#if defined(__arm__) && defined(__DEVKITARM__)
    if(!(ex & 1)) {
        if(!ex) {
            // Whole rows: one copy.
            dma_copy(from + (y0 + ey) * LINE, to + y0 * LINE, unsigned((y1 - y0) * LINE / 2), true);
            return;
        }
        const bool words = !(ex & 3) && !(x0 & 3) && !(pairs & 1);
        for(int y = y0; y < y1; ++y)
            dma_copy(from + (y + ey) * LINE + (x0 + ex) / 2, to + y * LINE + x0 / 2,
                     unsigned(words ? pairs / 2 : pairs), words);
        return;
    }
#endif
    for(int y = y0; y < y1; ++y) {
        uint16_t* out = to + y * LINE + x0 / 2;
        const int sx = x0 + ex;  // first source pixel
        if(!(sx & 1)) {
            const uint16_t* in = from + (y + ey) * LINE + sx / 2;
            if(!((reinterpret_cast<uintptr_t>(in) | reinterpret_cast<uintptr_t>(out)) & 3) && pairs >= 2) {
                const uint32_t* in32 = reinterpret_cast<const uint32_t*>(in);
                uint32_t* out32 = reinterpret_cast<uint32_t*>(out);
                int n = pairs / 2;
                for(; n >= 4; n -= 4, in32 += 4, out32 += 4) {
                    const uint32_t a = in32[0], b = in32[1], c = in32[2], d = in32[3];
                    out32[0] = a; out32[1] = b; out32[2] = c; out32[3] = d;
                }
                while(n--) *out32++ = *in32++;
                if(pairs & 1) *reinterpret_cast<uint16_t*>(out32) = *reinterpret_cast<const uint16_t*>(in32);
            } else {
                for(int i = 0; i < pairs; ++i) out[i] = in[i];
            }
        } else {
            // Odd move: each pair from the high byte of one source pair and the low byte of the next.
            const uint16_t* in = from + (y + ey) * LINE + (sx - 1) / 2;
            uint32_t carry = *in++ >> 8;
            for(int i = 0; i < pairs; ++i) {
                const uint32_t next = *in++;
                out[i] = uint16_t(carry | (next << 8));
                carry = next >> 8;
            }
        }
        // An odd move leaves one pixel beside the pairs.
        const uint16_t* source = from + (y + ey) * LINE;
        uint16_t* line = to + y * LINE;
        if(x0 > 0 && ((x0 - 1) + ex) >= 0) {
            const int x = x0 - 1, s = x + ex;
            const uint8_t v = uint8_t(s & 1 ? source[s >> 1] >> 8 : source[s >> 1]);
            set_high(line + (x >> 1), v);
        }
        if(x1 < W && x1 + ex < W) {
            const int x = x1, s = x + ex;
            const uint8_t v = uint8_t(s & 1 ? source[s >> 1] >> 8 : source[s >> 1]);
            set_low(line + (x >> 1), v);
        }
    }
}

}
