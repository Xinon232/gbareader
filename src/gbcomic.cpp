#include "gbcomic.h"

#include <cstring>

namespace reader::gbcomic {
namespace {

constexpr uint32_t HEADER_BYTES = 32;
constexpr uint32_t PICTURE_HEAD = 24;
constexpr uint32_t PALETTE_BYTES = COLORS * 2;
constexpr int MAX_VIEWS = 48;    // one per zoom (MAX_ZOOMS)
constexpr int MAX_RUN = 128;       // chunks of one strip read at a time
// Working memory: packed chunks of one strip, one unpacked chunk, and the
// chunk index of the picture drawn last (one read less for every part of it).
constexpr uint32_t PACKED_BYTES = 10 * 1024;
constexpr uint32_t CHUNK_BYTES = 2048;
constexpr uint32_t INDEX_ENTRIES = 1024;
static_assert(PACKED_BYTES + CHUNK_BYTES + INDEX_ENTRIES * 4 <= Book::ARENA_BYTES, "gbcomic working memory");

uint32_t get32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
uint16_t get16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t align4(uint32_t v) { return (v + 3) & ~3u; }

}

void zoom_name(const Zoom& zoom, char (&out)[16])
{
    const char* fixed = zoom.kind == ZoomKind::FIT_PAGE ? "Fit page" : zoom.factor == 100 ? "Fit width" : nullptr;
    int n = 0;
    if(fixed) {
        while(fixed[n] && n < 15) { out[n] = fixed[n]; ++n; }
        out[n] = 0;
        return;
    }
    const char* head = "Zoom ";
    while(*head) out[n++] = *head++;
    const int whole = zoom.factor / 100, part = zoom.factor % 100;
    if(whole >= 10) out[n++] = char('0' + whole / 10);
    out[n++] = char('0' + whole % 10);
    if(part) {
        out[n++] = '.';
        out[n++] = char('0' + part / 10);
        if(part % 10) out[n++] = char('0' + part % 10);
    }
    out[n++] = 'x';
    out[n] = 0;
}

const char* order_name(Order order)
{
    switch(order) {
    case Order::RIGHT_TO_LEFT: return "Right to left";
    case Order::COLUMNS: return "Columns";
    default: return "Left to right";
    }
}

const char* open_error_string(OpenError error)
{
    switch(error) {
    case OpenError::NOT_GBCOMIC: return "Not a .gbcomic file";
    case OpenError::NEWER_VERSION: return "Made for a newer gbareader";
    case OpenError::OLD_VERSION: return "Old .gbcomic: convert again";
    case OpenError::DAMAGED: return "Comic file damaged";
    case OpenError::READ: return "Book read failed";
    default: return "Book open failed";
    }
}

bool Book::read(uint32_t at, void* out, uint32_t count) const
{
    return at <= _size && count <= _size - at && _file->read_direct(at, static_cast<unsigned char*>(out), count);
}

bool Book::open(const ByteSource& file, uint8_t* memory, size_t memory_bytes)
{
    _open = false;
    _file = &file;
    _memory = memory;
    _index_cache = reinterpret_cast<uint32_t*>(memory + PACKED_BYTES + CHUNK_BYTES);
    _index_of = 0;
    _size = file.size();
    if(memory_bytes < ARENA_BYTES) { _error = OpenError::READ; return false; }
    uint8_t h[HEADER_BYTES + 4 * MAX_ZOOMS];
    if(_size < HEADER_BYTES) { _error = OpenError::NOT_GBCOMIC; return false; }
    if(!read(0, h, HEADER_BYTES)) { _error = OpenError::READ; return false; }
    if(std::memcmp(h, "GBCOMIC1", 8)) { _error = OpenError::NOT_GBCOMIC; return false; }
    const int version = get16(h + 8);
    if(version > 2) { _error = OpenError::NEWER_VERSION; return false; }
    if(version < 2) { _error = OpenError::OLD_VERSION; return false; }
    _pages = get16(h + 10);
    _orientations = h[12];
    _zoom_count = h[13];
    _page_table = get32(h + 16);
    const uint32_t header_size = get16(h + 14);
    if(!_pages || !(_orientations & 3) || !_zoom_count || _zoom_count > MAX_ZOOMS ||
       header_size != HEADER_BYTES + 4u * uint32_t(_zoom_count) || get32(h + 20) != _size ||
       _page_table < header_size || uint64_t(_page_table) + 4u * (uint32_t(_pages) + 1) > _size) {
        _error = OpenError::DAMAGED;
        return false;
    }
    if(!read(HEADER_BYTES, h + HEADER_BYTES, 4u * uint32_t(_zoom_count))) { _error = OpenError::READ; return false; }
    for(int i = 0; i < _zoom_count; ++i) {
        const uint8_t* z = h + HEADER_BYTES + 4 * i;
        if(z[0] > 1 || z[1] > 1 || !(_orientations & (1 << z[0])) || (z[1] == 1 && (get16(z + 2) < 50 || get16(z + 2) > 800))) {
            _error = OpenError::DAMAGED;
            return false;
        }
        _zooms[i].sideways = z[0] == 1;
        _zooms[i].kind = ZoomKind(z[1]);
        _zooms[i].factor = get16(z + 2);
    }
    _error = OpenError::NONE;
    _open = true;
    return true;
}

bool Book::view(int page, int zoom, Order order, View& v)
{
    v = View{};
    if(!_open || page < 0 || page >= _pages || zoom < 0 || zoom >= _zoom_count) return false;
    uint8_t b[8];
    if(!read(_page_table + 4u * uint32_t(page), b, 8)) return false;
    const uint32_t record = get32(b), end = get32(b + 4);
    if(!read(record, b, 8) || record >= end) return false;
    const int views = get16(b);
    if(views > MAX_VIEWS) return false;
    v.page_w = get16(b + 4) ? get16(b + 4) : 1;
    v.page_h = get16(b + 6) ? get16(b + 6) : 1;
    uint8_t entries[MAX_VIEWS * 8];
    if(!read(record + 8, entries, 8u * uint32_t(views))) return false;
    int found = -1;
    for(int i = 0; i < views; ++i)
        if(entries[8 * i] == uint8_t(zoom)) found = i;
    if(found < 0) return false;
    const int count = get16(entries + 8 * found + 2);
    const uint32_t at = get32(entries + 8 * found + 4);
    if(!count || count > MAX_PICTURES) return false;
    uint8_t picture_order[MAX_PICTURES];
    if(!read(at + uint32_t(order) * uint32_t(count), picture_order, uint32_t(count))) return false;
    const uint32_t offsets = align4(at + 3u * uint32_t(count));
    v.page = page;
    v.zoom = zoom;
    v.sideways = _zooms[zoom].sideways;
    v.order = order;
    v.screen_w = v.sideways ? 160 : 240;
    v.screen_h = v.sideways ? 240 : 160;
    v.picture_count = count;
    for(int i = 0; i < count; ++i) {
        if(picture_order[i] >= count) return false;
        uint8_t o[4], head[PICTURE_HEAD];
        if(!read(offsets + 4u * picture_order[i], o, 4) || !read(get32(o), head, PICTURE_HEAD)) return false;
        Picture& p = v.pictures[i];
        p.at = get32(o);
        p.w = get16(head); p.h = get16(head + 2);
        p.strip_w = get16(head + 4); p.strips = get16(head + 6);
        p.grid_w = get16(head + 8); p.grid_h = get16(head + 10);
        for(int k = 0; k < 4; ++k) p.box[k] = get16(head + 12 + 2 * k);
        p.chunk_rows = get16(head + 20);
        if(!p.w || !p.h || !p.strip_w || !p.chunk_rows || p.strips != (p.w + p.strip_w - 1) / p.strip_w ||
           uint32_t(p.strip_w) * p.chunk_rows > CHUNK_BYTES ||
           p.grid_w != (p.w + CELL - 1) / CELL || p.grid_h != (p.h + CELL - 1) / CELL) return false;
        p.ink_at = p.at + PICTURE_HEAD;
        p.palette_at = align4(p.ink_at + p.ink_bytes());
        p.index_at = p.palette_at + PALETTE_BYTES;
    }
    return true;
}

uint32_t Book::packed_bytes(const Picture& p, int strip, int first, int last) const
{
    if(!_index_cache || _index_of != p.at) return 0;
    const int chunks = (p.h + p.chunk_rows - 1) / p.chunk_rows;
    return _index_cache[strip * chunks + last + 1] - _index_cache[strip * chunks + first];
}

bool Book::ink(const Picture& p, uint8_t* bits, uint32_t max_bytes)
{
    return _open && p.ink_bytes() <= max_bytes && read(p.ink_at, bits, p.ink_bytes());
}

bool Book::palette(const Picture& p, uint16_t* out)
{
    return _open && read(p.palette_at, out, PALETTE_BYTES);   // little-endian, as the GBA
}

bool Book::draw_rect(const Picture& p, int x, int y, int w, int h, uint8_t* out, int stride)
{
    if(!_open || w <= 0 || h <= 0) return _open;
    for(int row = 0; row < h; ++row) {
        // Paper outside the picture.
        if(y + row < 0 || y + row >= p.h || x < 0 || x + w > p.w) std::memset(out + row * stride, 0, size_t(w));
    }
    const int chunk_rows = p.chunk_rows, strip_w = p.strip_w;
    const int chunks = (p.h + chunk_rows - 1) / chunk_rows;
    const int r0 = y > 0 ? y : 0, r1 = y + h < p.h ? y + h : p.h;
    if(r0 >= r1) return true;
    const int k0 = r0 / chunk_rows, k1 = (r1 - 1) / chunk_rows;
    uint8_t* packed = _memory;
    uint8_t* chunk = _memory + PACKED_BYTES;
    // The whole chunk index of a picture that fits, read once.
    const uint32_t entries = uint32_t(p.strips) * uint32_t(chunks) + 1;
    bool cached = _index_cache && _index_of == p.at;
    if(!cached && _index_cache && entries <= INDEX_ENTRIES) {
        _index_of = 0;
        if(!read(p.index_at, _index_cache, 4u * entries)) return false;
        for(uint32_t i = 0; i < entries; ++i) _index_cache[i] = get32(reinterpret_cast<const uint8_t*>(&_index_cache[i]));
        _index_of = p.at;
        cached = true;
    }
    for(int s = 0; s < p.strips; ++s) {
        const int sx0 = s * strip_w, sx1 = sx0 + strip_w < p.w ? sx0 + strip_w : p.w;
        const int a = sx0 > x ? sx0 : x, z = sx1 < x + w ? sx1 : x + w;
        if(a >= z) continue;
        const int width = sx1 - sx0;
        // This strip's chunks for the rows asked for lie one after the other: few reads.
        for(int kr = k0; kr <= k1; kr += MAX_RUN) {
            const int n = k1 - kr + 1 < MAX_RUN ? k1 - kr + 1 : MAX_RUN;
            uint32_t own[MAX_RUN + 1];
            const uint32_t* index = own;
            if(cached) index = _index_cache + (s * chunks + kr);
            else {
                if(!read(p.index_at + 4u * uint32_t(s * chunks + kr), own, 4u * uint32_t(n + 1))) return false;
                for(int i = 0; i <= n; ++i) own[i] = get32(reinterpret_cast<const uint8_t*>(&own[i]));
            }
            for(int i = 1; i <= n; ++i) if(index[i] < index[i - 1]) return false;
            const bool whole = index[n] - index[0] <= PACKED_BYTES;
            _bytes_read += index[n] - index[0];
            if(whole && !read(index[0], packed, index[n] - index[0])) return false;
            for(int k = kr; k < kr + n; ++k) {
                const uint32_t from = index[k - kr], bytes = index[k - kr + 1] - from;
                const uint8_t* source = packed + (from - index[0]);
                if(!whole) {
                    if(bytes > PACKED_BYTES || !read(from, packed, bytes)) return false;
                    source = packed;
                }
                const int cy0 = k * chunk_rows, cy1 = cy0 + chunk_rows < p.h ? cy0 + chunk_rows : p.h;
                if(!unpack(source, bytes, chunk, uint32_t(width) * uint32_t(cy1 - cy0))) return false;
                const int ya = cy0 > r0 ? cy0 : r0, yz = cy1 < r1 ? cy1 : r1;
                for(int yy = ya; yy < yz; ++yy)
                    std::memcpy(out + (yy - y) * stride + (a - x), chunk + (yy - cy0) * width + (a - sx0), size_t(z - a));
            }
        }
    }
    return true;
}

}
