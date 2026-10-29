#include "comic_region.h"

namespace reader::gbcomic {
namespace {

int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int floor_mod(int a, int b) { const int m = a % b; return m < 0 ? m + b : m; }

}

bool Region::start(const Picture& p, int screen_w, int screen_h)
{
    if(_picture && _at == p.at && _screen_w == screen_w && _screen_h == screen_h) {
        _picture = &p;
        return true;
    }
    _picture = nullptr;
    _screen_w = screen_w;
    _screen_h = screen_h;
    const int sw = p.strip_w, cr = p.chunk_rows;
    if(!sw || !cr || !_memory) return false;
    // Room for the screen at any place (one strip and one chunk row more than
    // it spans), and more where the memory allows.
    const int strips_needed = (screen_w + sw - 1) / sw + 1, chunks_needed = (screen_h + cr - 1) / cr + 1;
    int strips = strips_needed + 1;
    const int all_strips = p.strips, all_chunks = (p.h + cr - 1) / cr;
    if(strips > all_strips) strips = all_strips;
    if(strips > MAX_SLOTS) strips = MAX_SLOTS;
    int chunks = int(_bytes / (size_t(strips) * size_t(sw) * size_t(cr)));
    if(chunks > all_chunks) chunks = all_chunks;
    if(chunks > MAX_ROWS) chunks = MAX_ROWS;
    if(strips < (strips_needed < all_strips ? strips_needed : all_strips) ||
       chunks < (chunks_needed < all_chunks ? chunks_needed : all_chunks)) return false;
    _strips = strips;
    _chunks = chunks;
    _width = strips * sw;
    _height = chunks * cr;
    for(int i = 0; i < MAX_SLOTS; ++i) {
        _strip_of[i] = -1;
        for(int k = 0; k < MAX_ROWS; ++k) _chunk_of[i][k] = -1;
    }
    _picture = &p;
    _at = p.at;
    return true;
}

void Region::cover(int x, int y, int w, int h, int& s0, int& s1, int& k0, int& k1) const
{
    const Picture& p = *_picture;
    const int x0 = x < 0 ? 0 : x, x1 = x + w > p.w ? p.w : x + w;
    const int y0 = y < 0 ? 0 : y, y1 = y + h > p.h ? p.h : y + h;
    if(x0 >= x1 || y0 >= y1) { s0 = k0 = 0; s1 = k1 = -1; return; }
    s0 = x0 / p.strip_w; s1 = (x1 - 1) / p.strip_w;
    k0 = y0 / p.chunk_rows; k1 = (y1 - 1) / p.chunk_rows;
}

bool Region::load_rect(Book& book, int x, int y, int w, int h, int budget, uint32_t max_bytes, int& loaded)
{
    loaded = 0;
    if(!_picture) return false;
    const Picture& p = *_picture;
    int s0, s1, k0, k1;
    cover(x, y, w, h, s0, s1, k0, k1);
    const int sw = p.strip_w, cr = p.chunk_rows;
    for(int s = s0; s <= s1; ++s) {
        const int slot = s % _strips;
        if(_strip_of[slot] != s) {
            _strip_of[slot] = int16_t(s);
            for(int k = 0; k < _chunks; ++k) _chunk_of[slot][k] = -1;
        }
        const int width = (s + 1) * sw <= p.w ? sw : p.w - s * sw;
        for(int k = k0; k <= k1;) {
            if(_chunk_of[slot][k % _chunks] == k) { ++k; continue; }
            // A run of missing chunks that does not wrap round: one read.
            int end = k + 1;
            while(end <= k1 && end - k < budget - loaded && end % _chunks != 0 && _chunk_of[slot][end % _chunks] != end &&
                  book.packed_bytes(p, s, k, end) <= max_bytes) ++end;
            const uint32_t bytes = book.packed_bytes(p, s, k, end - 1);
            if(bytes > max_bytes) { loaded = -1; return true; }
            max_bytes -= bytes;
            const int top = k * cr, rows = (end * cr <= p.h ? end * cr : p.h) - top;
            uint8_t* to = _memory + size_t(k % _chunks) * cr * _width + size_t(slot) * sw;
            if(!book.draw_rect(p, s * sw, top, width, rows, to, _width)) return false;
            for(int i = k; i < end; ++i) _chunk_of[slot][i % _chunks] = int16_t(i);
            loaded += end - k;
            k = end;
            if(loaded >= budget) return true;
        }
    }
    return true;
}

bool Region::load_ahead(Book& book, int x, int y, int dx, int dy, int budget, uint32_t max_bytes)
{
    if(!_picture) return true;
    const Picture& p = *_picture;
    const int sw = p.strip_w, cr = p.chunk_rows;
    // First what the next frames bring in: the strips and chunk rows just
    // beside the screen, nearest first (loading far ones first leaves those
    // to draw()). Up to two, as many as fit beside a screen at any place.
    const int es = _strips - ((_screen_w + sw - 1) / sw + 1), ec = _chunks - ((_screen_h + cr - 1) / cr + 1);
    int loaded;
    for(int e = 1; e <= 2; ++e) {
        int nx = x, nw = _screen_w, ny = y, nh = _screen_h;
        if(dx && es > 0) { const int n = e < es ? e : es; nw += n * sw; if(dx < 0) nx -= n * sw; }
        if(dy && ec > 0) { const int n = e < ec ? e : ec; nh += n * cr; if(dy < 0) ny -= n * cr; }
        if(!load_rect(book, nx, ny, nw, nh, budget, max_bytes, loaded)) return true;
        if(loaded) return loaded < 0;
    }
    // Then the kept strips and chunk rows from the screen's on in the direction it goes.
    int rx = x, rw = _screen_w, ry = y, rh = _screen_h;
    if(dx > 0) { rx = floor_div(x, sw) * sw; rw = _strips * sw; }
    if(dx < 0) { const int end = (floor_div(x + _screen_w - 1, sw) + 1) * sw; rw = _strips * sw; rx = end - rw; }
    if(dy > 0) { ry = floor_div(y, cr) * cr; rh = _chunks * cr; }
    if(dy < 0) { const int end = (floor_div(y + _screen_h - 1, cr) + 1) * cr; rh = _chunks * cr; ry = end - rh; }
    if(!load_rect(book, rx, ry, rw, rh, budget, max_bytes, loaded)) return true;
    return loaded < budget;
}

bool Region::load_beside(Book& book, int x, int y, int rx, int ry, int budget, uint32_t max_bytes)
{
    if(!_picture) return true;
    int s0, s1, k0, k1, t0, t1, j0, j1;
    cover(x, y, _screen_w, _screen_h, s0, s1, k0, k1);
    cover(rx, ry, _screen_w, _screen_h, t0, t1, j0, j1);
    if(t1 < t0 || s1 < s0) return true;
    // The box of both screens (a slide to the next one passes over all of
    // it), cut down to what fits beside the screen shown: the part nearest
    // it, so loading never pushes out what is on the screen. What does not
    // fit is loaded while sliding.
    int a = s0 < t0 ? s0 : t0, b = s1 > t1 ? s1 : t1, c = k0 < j0 ? k0 : j0, d = k1 > j1 ? k1 : j1;
    if(b - a + 1 > _strips) {
        if(t0 < s0) a = s1 - _strips + 1;
        else b = s0 + _strips - 1;
    }
    if(d - c + 1 > _chunks) {
        if(j0 < k0) c = k1 - _chunks + 1;
        else d = k0 + _chunks - 1;
    }
    const Picture& p = *_picture;
    int loaded;
    if(!load_rect(book, a * p.strip_w, c * p.chunk_rows, (b - a + 1) * p.strip_w, (d - c + 1) * p.chunk_rows,
                  budget, max_bytes, loaded)) return true;
    return loaded < budget;
}

uint32_t Region::packed_missing(const Book& book, int x, int y, int w, int h) const
{
    if(!_picture) return 0;
    int s0, s1, k0, k1;
    cover(x, y, w, h, s0, s1, k0, k1);
    uint32_t bytes = 0;
    for(int s = s0; s <= s1; ++s)
        for(int k = k0; k <= k1; ++k) {
            const int slot = s % _strips;
            // More rows or strips than the memory holds: those are read on the way anyway.
            if(s1 - s0 >= _strips || k1 - k0 >= _chunks || _strip_of[slot] != s || _chunk_of[slot][k % _chunks] != k)
                bytes += book.packed_bytes(*_picture, s, k, k);
        }
    return bytes;
}

void Region::blit(int x, int y, int cx, int cy, int w, int h, uint16_t* screen, Angle angle) const
{
    if(w <= 0 || h <= 0) return;
    if(!_picture) { turn_fill(0, cx, cy, w, h, screen, angle); return; }
    const Picture& p = *_picture;
    // Picture rectangle shown, the paper around it.
    const int px0 = x + cx, py0 = y + cy;
    const int ix0 = px0 < 0 ? 0 : px0, ix1 = px0 + w > p.w ? p.w : px0 + w;
    const int iy0 = py0 < 0 ? 0 : py0, iy1 = py0 + h > p.h ? p.h : py0 + h;
    if(ix0 >= ix1 || iy0 >= iy1) { turn_fill(0, cx, cy, w, h, screen, angle); return; }
    if(iy0 > py0) turn_fill(0, cx, cy, w, iy0 - py0, screen, angle);
    if(iy1 < py0 + h) turn_fill(0, cx, iy1 - y, w, py0 + h - iy1, screen, angle);
    if(ix0 > px0) turn_fill(0, cx, iy0 - y, ix0 - px0, iy1 - iy0, screen, angle);
    if(ix1 < px0 + w) turn_fill(0, ix1 - x, iy0 - y, px0 + w - ix1, iy1 - iy0, screen, angle);
    // At most one wrap each way.
    for(int ya = iy0; ya < iy1;) {
        const int ty = floor_mod(ya, _height);
        const int yb = ya + (_height - ty) < iy1 ? ya + (_height - ty) : iy1;
        for(int xa = ix0; xa < ix1;) {
            const int tx = floor_mod(xa, _width);
            const int xb = xa + (_width - tx) < ix1 ? xa + (_width - tx) : ix1;
            turn_copy(_memory + size_t(ty) * _width + tx, _width, xa - x, ya - y, xb - xa, yb - ya, screen, angle);
            xa = xb;
        }
        ya = yb;
    }
}

}
