#pragma once
// .gbcomic books: comics prepared on a computer (the gbcomic conversion software). Each page
// has a view for each zoom the file lists (an angle and a size), each view
// one picture (or one per half spread at Fit page), each picture its own 248
// colours and a grid of where its ink is. Pixels are stored in vertical
// strips half a screen wide, 16 rows at a time, packed in the GBA BIOS LZ77
// format: any part of a picture is a few reads and unpacks, nothing is
// scaled. Where the screens go is the reader's choice (comic_layout.h).
// Format: docs/gbcomic-format.md.
#include "reader_core.h"

#include <cstddef>
#include <cstdint>

namespace reader::gbcomic {

constexpr int COLORS = 248;        // screen colours FIRST_COLOR.. (0..7: the reader's)
constexpr int FIRST_COLOR = 8;
constexpr int MAX_ZOOMS = 48;      // Fit page, Fit width and 1.1x..3.0x for both angles: 44
constexpr int MAX_PICTURES = 8;    // per view
constexpr int CELL = 8;            // ink grid: one bit per CELL x CELL pixels

// A zoom of the file: Fit page, or a picture width in hundredths of the
// screen's (100: Fit width).
enum class ZoomKind : uint8_t { FIT_PAGE = 0, WIDTH = 1 };
struct Zoom {
    bool sideways = false;
    ZoomKind kind = ZoomKind::FIT_PAGE;
    uint16_t factor = 0;
};
// "Fit page", "Fit width", "Zoom 1.6x", "Zoom 2.25x".
void zoom_name(const Zoom& zoom, char (&out)[16]);

enum class Order : uint8_t { LEFT_TO_RIGHT = 0, RIGHT_TO_LEFT = 1, COLUMNS = 2 };
constexpr int ORDER_COUNT = 3;
const char* order_name(Order order);

enum class OpenError : uint8_t { NONE, NOT_GBCOMIC, NEWER_VERSION, OLD_VERSION, DAMAGED, READ };
const char* open_error_string(OpenError error);

struct Picture {
    uint32_t at = 0;                 // its head in the file
    uint16_t w = 0, h = 0;
    uint16_t strip_w = 0, strips = 0, chunk_rows = 0;
    uint16_t grid_w = 0, grid_h = 0; // ink grid, in cells
    uint16_t box[4] = {0, 0, 4096, 4096};  // where on the page, 1/4096
    uint32_t ink_at = 0, palette_at = 0, index_at = 0;
    uint32_t ink_bytes() const { return (uint32_t(grid_w) * grid_h + 7) / 8; }
};

// One page at one zoom: its pictures in reading order.
struct View {
    int page = -1;
    int zoom = -1;                   // in the file's list
    bool sideways = false;
    Order order = Order::LEFT_TO_RIGHT;
    int screen_w = 240, screen_h = 160;  // as the reader sees the screen
    uint16_t page_w = 1, page_h = 1;     // the original page, for the map
    int picture_count = 0;
    Picture pictures[MAX_PICTURES];
};

class Book {
public:
    // memory: at least ARENA_BYTES, 4-byte aligned (reading and unpacking).
    static constexpr size_t ARENA_BYTES = 16 * 1024;
    bool open(const ByteSource& file, uint8_t* memory, size_t memory_bytes);
    void close() { _open = false; }
    bool is_open() const { return _open; }
    OpenError error() const { return _error; }
    int page_count() const { return _pages; }
    bool has_sideways() const { return _orientations & 2; }
    bool has_upright() const { return _orientations & 1; }
    int zoom_count() const { return _zoom_count; }
    const Zoom& zoom(int index) const { return _zooms[index]; }

    // False if the page has no view for that zoom (or cannot be read).
    bool view(int page, int zoom, Order order, View& view);
    // The picture's ink grid (bit i: cell i, row by row); false if it does not fit.
    bool ink(const Picture& picture, uint8_t* bits, uint32_t max_bytes);
    // Its colours (for screen colours FIRST_COLOR onwards), BGR555.
    bool palette(const Picture& picture, uint16_t* out);
    // The w x h pixels at (x, y) of the picture into out (stride bytes a row):
    // screen colour indices, 0 (paper white) outside the picture.
    bool draw_rect(const Picture& picture, int x, int y, int w, int h, uint8_t* out, int stride);

    // Packed bytes of chunks first..last of a strip, if its index is at hand (else 0).
    uint32_t packed_bytes(const Picture& picture, int strip, int first, int last) const;
    // Packed bytes read so far (for the reading speed).
    uint32_t bytes_read() const { return _bytes_read; }

    // Unpacks GBA BIOS LZ77 (type 0x10); false if damaged or larger than out_bytes.
    static bool unpack(const uint8_t* in, uint32_t in_bytes, uint8_t* out, uint32_t out_bytes);

private:
    bool read(uint32_t at, void* out, uint32_t count) const;

    const ByteSource* _file = nullptr;
    uint8_t* _memory = nullptr;
    bool _open = false;
    OpenError _error = OpenError::NONE;
    int _pages = 0;
    uint8_t _orientations = 0;
    int _zoom_count = 0;
    Zoom _zooms[MAX_ZOOMS];
    uint32_t _page_table = 0, _size = 0;
    uint32_t* _index_cache = nullptr;
    uint32_t _index_of = 0;    // the picture whose index it holds (its file offset)
    uint32_t _bytes_read = 0;
};

}
