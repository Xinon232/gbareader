#pragma once
// The comic page on screen: which screen is shown, moving between screens
// (at once or sliding), moving around the picture with the D-pad, and
// drawing only what changes. The screen is never drawn whole while it moves: the
// picture on show is copied over by the move and the part coming in is
// drawn from the decoded region kept around it (comic_region.h).
#include "comic_layout.h"
#include "comic_region.h"
#include "reader_book_prefs.h"

#include <cstddef>
#include <cstdint>

namespace reader {

class ComicReader {
public:
    static constexpr int CELL_BYTES = 2048;   // ink grid (larger pictures go without)

    void set_memory(uint8_t* region, size_t bytes) { _region.set_memory(region, bytes); }
    // How many packed bytes the card reads (and unpacks) in a frame: a slide
    // that has to load on its way takes as many frames as that needs.
    void set_read_speed(uint32_t bytes_per_frame) { _read_speed = bytes_per_frame ? bytes_per_frame : 1; }
    // A page at a zoom, from its first screen with something on it (or the
    // last, going back). False: the page cannot be read.
    bool open_page(gbcomic::Book& book, int page, int zoom, gbcomic::Order order, uint8_t counts,
                   bool skip_empty, bool page_map, bool at_end);
    int page() const { return _view.page; }
    bool page_open() const { return _open; }
    // Standing still (no slide, no D-pad move just now): what is loaded now is the next screen.
    bool waiting() const { return _open && !_anim && _moved_frames == 0; }
    // Settings or another screen covered the comic: draw it all again.
    // A slide on its way ends at its screen (what it had loaded is gone).
    void redraw() { finish_anim(); _full = true; _map_on_screen = false; }
    // After a change of the screens' memory (another screen drew there).
    void forget_region() { finish_anim(); _region.forget(); _full = true; _map_on_screen = false; }

    // A / B. The screens are a fixed grid; after the D-pad moved the view,
    // A goes on from where it is: to the next screen of the same row that
    // starts beyond it (keeping its height), or, at the row's end, to the
    // first screen of the next row of the grid. B the same way back. Right
    // to left mirrored; Columns reads down the columns. along: the slide
    // within a row; line: the slide to another row.
    enum class Step : uint8_t { NONE, MOVED, NEXT_PAGE, PREVIOUS_PAGE };
    Step forward(Slide along, Slide line) { return step(1, along, line); }
    Step back(Slide along, Slide line) { return step(-1, along, line); }
    // D-pad: the view goes (dx, dy) further, up to the picture's edges.
    // The grid stays as it is.
    void move_by(int dx, int dy);
    bool moving() const { return _anim; }
    // The middle of the screen on the page (1/4096 of its width and height),
    // and the view put with its middle there (after another zoom: L / R held),
    // on the screen of the grid nearest to it. False: no page open.
    bool view_center(int& px, int& py) const;
    void center_on(int px, int py);

    // Every frame: draws what changed onto the hidden screen page (from the
    // one shown). True: flip. colors gets the picture's colours when they
    // change (colors_changed). failed: the book could not be read.
    bool draw(gbcomic::Book& book, uint16_t* hidden, const uint16_t* shown, Angle angle,
              uint16_t* colors, bool& colors_changed, bool& failed);
    // Loads ahead: at most budget chunks and max_bytes packed bytes (the
    // time left); false when done or out of time.
    bool prefetch(gbcomic::Book& book, int budget, uint32_t max_bytes = ~0u);

    // The screens of a page at a zoom, for the View screen: count, the
    // view and each picture's grid.
    static int layout(gbcomic::Book& book, int page, int zoom, gbcomic::Order order, uint8_t counts,
                      gbcomic::View& view, comic_layout::Grid* grids);
    // The part of the page a screen at (x, y) of picture shows (1/4096 of the page).
    static void page_box(const gbcomic::View& view, int picture, int x, int y, int (&box)[4]);

    // For tests: the screen shown.
    int x() const { return _x; }
    int y() const { return _y; }
    int tile() const { return _tile; }
    int tile_count() const { return _tiles; }
    const gbcomic::View& view() const { return _view; }
    const comic_layout::Grid& grid(int picture) const { return _grids[picture]; }
    bool map_shown() const { return _map_on_screen; }
    // The map's place on the screen (as the reader sees it).
    void map_rect(int& mx, int& my, int& mw, int& mh) const;

private:
    // A place to show: a screen of the grid and where the view is.
    struct Place {
        comic_layout::Spot spot;
        int x = 0, y = 0;
    };
    enum class Reach : uint8_t { ALONG, LINE, PICTURE, END };
    Step step(int dir, Slide along, Slide line);
    Reach next_place(const Place& from, int dir, Place& to) const;
    // Where A (1) or B (-1) goes from the view now (screens with nothing but
    // paper passed over); false at the page's end or on another picture.
    bool next_target(int dir, Place& to) const;
    bool empty_at(const Place& place) const;
    bool skip(int tile) const;
    void tile_xy(int tile, int& x, int& y) const;
    void go_to(const Place& place, Slide slide);
    void finish_anim();
    // Where a slide is on frame (its target from the last frame on).
    void anim_at(int frame, int& x, int& y) const;
    void draw_map(uint16_t* hidden, Angle angle) const;
    comic_layout::Cells ink_cells() const;

    gbcomic::View _view;
    comic_layout::Grid _grids[gbcomic::MAX_PICTURES];
    gbcomic::Region _region;
    bool _open = false;
    int _tiles = 0, _tile = 0;
    comic_layout::Spot _spot;
    int _x = 0, _y = 0;                 // the screen being shown (moving: where it is now)
    bool _skip_empty = true, _page_map = true;
    // Ink of the picture (one-picture views).
    uint8_t _ink[CELL_BYTES];
    bool _cells = false;
    // What the shown screen page has.
    bool _full = true, _colors_set = false;
    int _shown_x = 0, _shown_y = 0, _shown_picture = -1;
    bool _map_on_screen = false;
    int _map_frames = 0, _still_frames = 0;
    static constexpr int MAP_MAX = 56;
    uint8_t _under_map[MAP_MAX * MAP_MAX];
    int _map_x = 0, _map_y = 0, _map_w = 0, _map_h = 0;   // of what _under_map holds
    // Sliding from one screen to the next. _anim_follow: loading what each
    // frame shows (after a move, or to another row), not only the target.
    // _anim_chained: it took over from a slide on its way (Smooth starts at speed).
    bool _anim = false, _anim_smooth = false, _anim_loaded = false, _anim_follow = false, _anim_paced = false;
    bool _anim_chained = false;
    uint32_t _read_speed = 4096;
    int _from_x = 0, _from_y = 0, _to_x = 0, _to_y = 0, _anim_frame = 0, _anim_frames = 0;
    int _last_dx = 0, _last_dy = 0, _moved_frames = 0;
    int _last_dir = 1;   // A (1) or B (-1) last: the screen to load while waiting
};

}
