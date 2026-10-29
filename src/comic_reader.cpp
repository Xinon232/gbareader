#include "comic_reader.h"

#include <cstring>

namespace reader {
namespace {

using namespace comic_layout;

int clamp(int v, int low, int high) { return v < low ? low : v > high ? high : v; }
int absolute(int v) { return v < 0 ? -v : v; }
int sign(int v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
// From at towards to: the whole way when it is 1 pixel, else an even step
// (an odd one a pixel shorter).
int even_step(int at, int to) { const int d = to - at; return at + (d > 1 || d < -1 ? d - d % 2 : d); }

constexpr int MAP_SIZE = 44;
constexpr int MAP_FRAMES = 70;
constexpr int MAP_STILL_FRAMES = 10;
constexpr int MAX_SLIDE_FRAMES = 60;   // a slide paced to the card: at most a second

}

int ComicReader::layout(gbcomic::Book& book, int page, int zoom, gbcomic::Order order, uint8_t counts,
                        gbcomic::View& v, Grid* grids)
{
    if(!book.view(page, zoom, order, v)) return 0;
    // Counts are for one picture; the halves of a spread fit the screen anyway.
    const bool one = v.picture_count == 1;
    for(int i = 0; i < v.picture_count; ++i)
        make_grid(v.pictures[i].w, v.pictures[i].h, v.screen_w, v.screen_h,
                  one ? counts >> 4 : 0, one ? counts & 15 : 0, grids[i]);
    return spot_count(grids, v.picture_count);
}

void ComicReader::page_box(const gbcomic::View& v, int picture, int x, int y, int (&box)[4])
{
    const gbcomic::Picture& p = v.pictures[picture];
    const int x0 = x < 0 ? 0 : x, x1 = x + v.screen_w > p.w ? p.w : x + v.screen_w;
    const int y0 = y < 0 ? 0 : y, y1 = y + v.screen_h > p.h ? p.h : y + v.screen_h;
    const int bw = p.box[2] - p.box[0], bh = p.box[3] - p.box[1];
    box[0] = p.box[0] + bw * x0 / p.w;
    box[2] = p.box[0] + bw * x1 / p.w;
    box[1] = p.box[1] + bh * y0 / p.h;
    box[3] = p.box[1] + bh * y1 / p.h;
}

Cells ComicReader::ink_cells() const
{
    Cells c;
    if(_cells) { c.bits = const_cast<uint8_t*>(_ink); c.w = _view.pictures[0].grid_w; c.h = _view.pictures[0].grid_h; }
    return c;
}

void ComicReader::tile_xy(int tile, int& x, int& y) const
{
    const Spot s = spot_at(_grids, _view.picture_count, _view.order, tile);
    x = _grids[s.picture].xs[s.column];
    y = _grids[s.picture].ys[s.row];
}

// Screens with nothing but paper (Skip empty).
bool ComicReader::skip(int tile) const
{
    if(!_cells || !_skip_empty) return false;
    int x, y;
    tile_xy(tile, x, y);
    return !any_ink(ink_cells(), x, y, _view.screen_w, _view.screen_h);
}

bool ComicReader::open_page(gbcomic::Book& book, int page, int zoom, gbcomic::Order order, uint8_t counts,
                            bool skip_empty, bool page_map, bool at_end)
{
    _open = _anim = false;
    _skip_empty = skip_empty;
    _page_map = page_map;
    _tiles = layout(book, page, zoom, order, counts, _view, _grids);
    if(!_tiles) return false;
    _cells = _view.picture_count == 1 && _view.pictures[0].ink_bytes() <= CELL_BYTES &&
             book.ink(_view.pictures[0], _ink, CELL_BYTES);
    _open = true;
    _last_dir = at_end ? -1 : 1;
    if(at_end) {
        _tile = _tiles - 1;
        while(_tile > 0 && skip(_tile)) --_tile;
    } else {
        _tile = 0;
        while(_tile + 1 < _tiles && skip(_tile)) ++_tile;
    }
    _spot = spot_at(_grids, _view.picture_count, _view.order, _tile);
    tile_xy(_tile, _x, _y);
    _full = true;
    _map_on_screen = false;
    _map_frames = page_map && _tiles > 1 ? MAP_FRAMES : 0;
    _moved_frames = 0;
    return true;
}

void ComicReader::finish_anim()
{
    if(!_anim) return;
    _x = _to_x;
    _y = _to_y;
    _anim = false;
}

bool ComicReader::empty_at(const Place& place) const
{
    return _cells && _skip_empty && place.spot.picture == 0 &&
           !any_ink(ink_cells(), place.x, place.y, _view.screen_w, _view.screen_h);
}

ComicReader::Reach ComicReader::next_place(const Place& from, int dir, Place& to) const
{
    const gbcomic::Order order = _view.order;
    const bool down = order == gbcomic::Order::COLUMNS;
    const Grid& g = _grids[from.spot.picture];
    // Along the line (a row, or a column for Columns): positions grow with the index.
    const int16_t* along = down ? g.ys : g.xs;
    const int16_t* across = down ? g.xs : g.ys;
    const int count = down ? g.rows : g.columns, lines = down ? g.columns : g.rows;
    const int size = down ? _view.screen_w : _view.screen_h;
    const int at = down ? from.y : from.x, cross = down ? from.x : from.y;
    const int reading = down || order == gbcomic::Order::LEFT_TO_RIGHT ? 1 : -1;
    const int want = reading * dir;
    to = from;
    // The next screen of this line after this one and beyond the view
    // (keeping its height).
    const int here = down ? from.spot.row : from.spot.column;
    int k = -1;
    if(want > 0) { for(int i = here + 1; i < count; ++i) if(along[i] > at) { k = i; break; } }
    else { for(int i = here - 1; i >= 0; --i) if(along[i] < at) { k = i; break; } }
    if(k >= 0) {
        (down ? to.spot.row : to.spot.column) = k;
        (down ? to.y : to.x) = along[k];
        return Reach::ALONG;
    }
    // The next line of the grid, from its first screen (its last going back).
    const int line = (down ? from.spot.column : from.spot.row) + dir;
    if(line >= 0 && line < lines) {
        const int first = reading > 0 ? 0 : count - 1, last = reading > 0 ? count - 1 : 0;
        const int index = dir > 0 ? first : last;
        (down ? to.spot.column : to.spot.row) = line;
        (down ? to.spot.row : to.spot.column) = index;
        (down ? to.y : to.x) = along[index];
        // Its own height, or lower if the view was moved down past it (higher, going back).
        const int grid = across[line];
        int c = dir > 0 ? clamp(cross, grid, grid + size / 2) : clamp(cross, grid - size / 2, grid);
        int min_x, max_x, min_y, max_y;
        limits(g, min_x, max_x, min_y, max_y);
        c = down ? clamp(c, min_x, max_x) : clamp(c, min_y, max_y);
        (down ? to.x : to.y) = c;
        return Reach::LINE;
    }
    // The next picture (half of a spread), from its first screen (last going back).
    const int picture = from.spot.picture + dir;
    if(picture < 0 || picture >= _view.picture_count) return Reach::END;
    int index = 0;
    for(int i = 0; i < picture; ++i) index += _grids[i].columns * _grids[i].rows;
    if(dir < 0) index += _grids[picture].columns * _grids[picture].rows - 1;
    to.spot = spot_at(_grids, _view.picture_count, order, index);
    to.x = _grids[picture].xs[to.spot.column];
    to.y = _grids[picture].ys[to.spot.row];
    return Reach::PICTURE;
}

bool ComicReader::next_target(int dir, Place& to) const
{
    to.spot = _spot;
    to.x = _x;
    to.y = _y;
    for(int guard = 0; guard <= _tiles; ++guard) {
        Place next;
        const Reach r = next_place(to, dir, next);
        if(r == Reach::END || r == Reach::PICTURE) return false;
        to = next;
        if(!empty_at(to)) return true;
    }
    return false;
}

bool ComicReader::view_center(int& px, int& py) const
{
    if(!_open) return false;
    // Sliding: the screen it goes to (the grid's, not one on the way).
    int b[4];
    page_box(_view, _spot.picture, _anim ? _to_x : _x, _anim ? _to_y : _y, b);
    px = (b[0] + b[2]) / 2;
    py = (b[1] + b[3]) / 2;
    return true;
}

void ComicReader::center_on(int px, int py)
{
    if(!_open) return;
    finish_anim();
    // The picture with the point (the half of a spread it is on).
    int picture = 0;
    for(int i = 0; i < _view.picture_count; ++i) {
        const gbcomic::Picture& p = _view.pictures[i];
        if(px >= p.box[0] && px < p.box[2]) { picture = i; break; }
    }
    const gbcomic::Picture& p = _view.pictures[picture];
    const Grid& g = _grids[picture];
    const int bw = p.box[2] - p.box[0] > 0 ? p.box[2] - p.box[0] : 1, bh = p.box[3] - p.box[1] > 0 ? p.box[3] - p.box[1] : 1;
    int min_x, max_x, min_y, max_y;
    limits(g, min_x, max_x, min_y, max_y);
    _x = clamp(int((int64_t(px) - p.box[0]) * p.w / bw) - _view.screen_w / 2, min_x, max_x);
    _y = clamp(int((int64_t(py) - p.box[1]) * p.h / bh) - _view.screen_h / 2, min_y, max_y);
    // The screen of the grid nearest the view (A / B go on from there).
    Spot s;
    s.picture = picture;
    for(int c = 0; c < g.columns; ++c) if(absolute(g.xs[c] - _x) < absolute(g.xs[s.column] - _x)) s.column = c;
    for(int r = 0; r < g.rows; ++r) if(absolute(g.ys[r] - _y) < absolute(g.ys[s.row] - _y)) s.row = r;
    _spot = s;
    _tile = spot_index(_grids, _view.picture_count, _view.order, s);
    _full = true;
    _map_on_screen = false;
    if(_page_map && _tiles > 1) _map_frames = MAP_FRAMES;
}

ComicReader::Step ComicReader::step(int dir, Slide along, Slide line)
{
    if(!_open) return Step::NONE;
    _last_dir = dir;
    // The grid goes on from where a slide goes (_spot is its screen already).
    Place to;
    to.spot = _spot;
    to.x = _anim ? _to_x : _x;
    to.y = _anim ? _to_y : _y;
    bool same_line_only = true, other_picture = false;
    // Screens with nothing but paper are passed over.
    for(int guard = 0; guard <= _tiles; ++guard) {
        Place next;
        const Reach r = next_place(to, dir, next);
        if(r == Reach::END) return dir > 0 ? Step::NEXT_PAGE : Step::PREVIOUS_PAGE;
        if(r != Reach::ALONG) same_line_only = false;
        if(r == Reach::PICTURE) other_picture = true;
        to = next;
        if(!empty_at(to)) break;
    }
    go_to(to, other_picture ? Slide::STATIC : same_line_only ? along : line);
    return Step::MOVED;
}

void ComicReader::go_to(const Place& place, Slide slide)
{
    // A slide on its way: the next one starts from where the screen is.
    const bool chained = _anim;
    _anim = false;
    const Spot s = place.spot;
    const int tx = place.x, ty = place.y;
    const int dx = tx - _x, dy = ty - _y;
    // One way, less than a screen, from what the screen shows: everything
    // that comes in is on the target screen. Any other way (also after a
    // slide cut short), each frame loads what it shows.
    const bool straight = (!dx || !dy) && absolute(dx) < _view.screen_w && absolute(dy) < _view.screen_h &&
                          _x == _shown_x && _y == _shown_y;
    const bool slide_now = slide != Slide::STATIC && s.picture == _spot.picture && (dx || dy) && !_full;
    _tile = spot_index(_grids, _view.picture_count, _view.order, s);
    _spot = s;
    if(slide_now) {
        _anim = true;
        _anim_smooth = slide == Slide::SMOOTH;
        _anim_chained = chained;
        _anim_loaded = false;
        _anim_follow = !straight;
        _anim_paced = false;
        _last_dx = sign(dx);
        _last_dy = sign(dy);
        _from_x = _x; _from_y = _y;
        _to_x = tx; _to_y = ty;
        const int distance = absolute(dx) > absolute(dy) ? absolute(dx) : absolute(dy);
        _anim_frame = 0;
        _anim_frames = _anim_smooth ? (distance + 6) / 7 : (distance + 9) / 10;
        if(_anim_frames < 4) _anim_frames = 4;
    } else {
        _x = tx;
        _y = ty;
    }
    if(_page_map) _map_frames = MAP_FRAMES;
    _moved_frames = 0;
}

void ComicReader::move_by(int dx, int dy)
{
    if(!_open || (!dx && !dy)) return;
    // A slide stops where the screen is.
    _anim = false;
    int min_x, max_x, min_y, max_y;
    limits(_grids[_spot.picture], min_x, max_x, min_y, max_y);
    const int nx = clamp(_x + dx, min_x, max_x), ny = clamp(_y + dy, min_y, max_y);
    if(nx == _x && ny == _y) return;
    _last_dx = sign(nx - _x);
    _last_dy = sign(ny - _y);
    // The way the reader heads (what to load while waiting): along a row
    // forward (right; left reading right to left) or down to the next row is
    // where A goes, the other way B (Columns: down a column, or right).
    const bool down = _view.order == gbcomic::Order::COLUMNS;
    const int along = down ? _last_dy : _view.order == gbcomic::Order::RIGHT_TO_LEFT ? -_last_dx : _last_dx;
    const int across = down ? _last_dx : _last_dy;
    if(along || across) _last_dir = along ? along : across;
    _moved_frames = 10;
    _x = nx;
    _y = ny;
    if(_page_map) _map_frames = MAP_FRAMES;
}

void ComicReader::map_rect(int& mx, int& my, int& mw, int& mh) const
{
    const int pw = _view.page_w, ph = _view.page_h;
    int w = pw >= ph ? MAP_SIZE : MAP_SIZE * pw / ph, h = pw >= ph ? MAP_SIZE * ph / pw : MAP_SIZE;
    if(w < 4) w = 4;
    if(h < 4) h = 4;
    mw = w + 6;
    mh = h + 6;
    mx = _view.screen_w - w - 9;
    my = 3;
}

void ComicReader::draw_map(uint16_t* hidden, Angle angle) const
{
    int mx, my, mw, mh;
    map_rect(mx, my, mw, mh);
    const int x0 = mx + 3, y0 = my + 3, w = mw - 6, h = mh - 6;
    turn_fill(1, mx, my, mw, mh, hidden, angle);
    turn_fill(0, x0 - 2, y0 - 2, w + 4, h + 4, hidden, angle);
    turn_fill(5, x0, y0, w, h, hidden, angle);
    turn_fill(0, x0 + 1, y0 + 1, w - 2, h - 2, hidden, angle);
    // The screen's place, inside the frame (at the page's edges it never covers it).
    int b[4];
    page_box(_view, _spot.picture, _x, _y, b);
    const int iw = w - 2, ih = h - 2;
    int tx = b[0] * iw / 4096, tw = (b[2] - b[0]) * iw / 4096;
    int ty = b[1] * ih / 4096, th = (b[3] - b[1]) * ih / 4096;
    if(tw < 2) tw = 2;
    if(th < 2) th = 2;
    tx = clamp(tx, 0, iw - tw);
    ty = clamp(ty, 0, ih - th);
    tx += x0 + 1;
    ty += y0 + 1;
    turn_fill(1, tx, ty, tw, th, hidden, angle);
    if(tw > 4 && th > 4) turn_fill(4, tx + 1, ty + 1, tw - 2, th - 2, hidden, angle);
}

bool ComicReader::draw(gbcomic::Book& book, uint16_t* hidden, const uint16_t* shown, Angle angle,
                       uint16_t* colors, bool& colors_changed, bool& failed)
{
    colors_changed = failed = false;
    if(!_open) return false;
    const int sw = _view.screen_w, sh = _view.screen_h;
    const gbcomic::Picture& p = _view.pictures[_spot.picture];
    const bool whole = _full || _shown_picture != _spot.picture || !_region.started();
    if(whole && !_region.start(p, sw, sh)) { failed = true; return false; }
    if(_anim && _anim_follow && !_anim_paced) {
        // A slide that loads on its way (to another row): as long as reading
        // what is not in memory yet takes, at an even speed (reading keeps
        // up all the way), so it moves evenly instead of stopping now and then.
        // What is loaded already lies near the start, so the reading piles up
        // towards the end: every eighth of the way must be read by the time
        // the screen gets there.
        _anim_paced = true;
        const uint32_t per_frame = _read_speed + 1;
        int needed = 0;
        for(int i = 1; i <= 8; ++i) {
            const int px = _from_x + (_to_x - _from_x) * i / 8, py = _from_y + (_to_y - _from_y) * i / 8;
            const int x0 = _from_x < px ? _from_x : px, y0 = _from_y < py ? _from_y : py;
            const int x1 = (_from_x > px ? _from_x : px) + _view.screen_w;
            const int y1 = (_from_y > py ? _from_y : py) + _view.screen_h;
            const uint32_t missing = _region.packed_missing(book, x0, y0, x1 - x0, y1 - y0);
            const int frames = int(missing * 8 / (per_frame * uint32_t(i))) + 1;
            if(frames > needed) needed = frames;
        }
        if(needed > _anim_frames) {
            _anim_frames = needed > MAX_SLIDE_FRAMES ? MAX_SLIDE_FRAMES : needed;
            _anim_smooth = false;
        }
    }
    if(_anim) {
        // Along a row everything that comes in is on the screen it goes to.
        if(!_anim_loaded && !_anim_follow) {
            if(!_region.load(book, _to_x, _to_y)) { failed = true; return false; }
            _anim_loaded = true;
        }
        if(++_anim_frame >= _anim_frames) finish_anim();
        else {
            // Even steps across the screen (an odd one is a slow copy, see
            // turn_shift): a step of 2 pixels or more is made even, so the
            // screen stays within a pixel of the curve; 1 pixel moves
            // anyway (Smooth starting and stopping). The last frame lands
            // on the target.
            int ax, ay;
            anim_at(_anim_frame, ax, ay);
            _x = even_step(_x, ax);
            _y = even_step(_y, ay);
        }
    }
    if(_moved_frames > 0) --_moved_frames;
    if(_map_frames > 0) --_map_frames;
    // The map shows once the screen keeps still (moving it with the screen costs too much).
    // (A screen drawn whole keeps still: the map shown with it stays.)
    if(_anim) _still_frames = 0;
    else if(whole) _still_frames = MAP_STILL_FRAMES;
    else if(_x != _shown_x || _y != _shown_y) _still_frames = 0;
    else if(_still_frames < MAP_STILL_FRAMES) ++_still_frames;
    const bool want_map = _page_map && _map_frames > 0 && (_still_frames >= MAP_STILL_FRAMES || whole);
    if(whole) {
        if(!_region.load(book, _x, _y) || !book.palette(p, colors)) { failed = true; return false; }
        colors_changed = true;
        _region.blit(_x, _y, 0, 0, sw, sh, hidden, angle);
        _full = false;
        _shown_picture = _spot.picture;
    } else {
        if(_x == _shown_x && _y == _shown_y && want_map == _map_on_screen) return false;
        if((!_anim || _anim_follow) && !_region.load(book, _x, _y)) { failed = true; return false; }
        const int dx = _x - _shown_x, dy = _y - _shown_y;
        if(absolute(dx) >= sw || absolute(dy) >= sh) {
            _region.blit(_x, _y, 0, 0, sw, sh, hidden, angle);
        } else {
            // The screen shown, moved; what comes in from the region.
            turn_shift(shown, hidden, angle, dx, dy);
            if(dx > 0) _region.blit(_x, _y, sw - dx, 0, dx, sh, hidden, angle);
            if(dx < 0) _region.blit(_x, _y, 0, 0, -dx, sh, hidden, angle);
            if(dy > 0) _region.blit(_x, _y, 0, sh - dy, sw, dy, hidden, angle);
            if(dy < 0) _region.blit(_x, _y, 0, 0, sw, -dy, hidden, angle);
            // The map moved with it: what it covered goes back.
            if(_map_on_screen)
                turn_copy(_under_map, _map_w, _map_x - dx, _map_y - dy, _map_w, _map_h, hidden, angle);
        }
    }
    _shown_x = _x;
    _shown_y = _y;
    _map_on_screen = false;
    if(want_map) {
        map_rect(_map_x, _map_y, _map_w, _map_h);
        turn_read(hidden, angle, _map_x, _map_y, _map_w, _map_h, _under_map, _map_w);
        draw_map(hidden, angle);
        _map_on_screen = true;
    }
    return true;
}

void ComicReader::anim_at(int frame, int& x, int& y) const
{
    if(frame >= _anim_frames) { x = _to_x; y = _to_y; return; }
    int t = frame * 256 / _anim_frames;
    // Smoothstep, 0..256. A slide that takes over from another one starts at
    // speed (the second half of the curve: no easing in).
    if(_anim_smooth && _anim_chained) { const int u = 128 + t / 2; t = (u * u * (768 - 2 * u) / 65536 - 128) * 2; }
    else if(_anim_smooth) t = t * t * (768 - 2 * t) / 65536;
    x = _from_x + (_to_x - _from_x) * t / 256;
    y = _from_y + (_to_y - _from_y) * t / 256;
}

bool ComicReader::prefetch(gbcomic::Book& book, int budget, uint32_t max_bytes)
{
    if(!_open || _full || !_region.started() || (_anim && !_anim_follow)) return false;
    if(_anim) return !_region.load_ahead(book, _x, _y, _last_dx, _last_dy, budget, max_bytes);
    if(_moved_frames > 0) return !_region.load_ahead(book, _x, _y, _last_dx, _last_dy, budget, max_bytes);
    // Waiting: exactly where the next A or B goes from the view now (after a
    // D-pad move that is not the next screen of the grid) - the way the last
    // one went, the other way at the page's start or end - on this picture.
    Place to;
    if(!next_target(_last_dir, to) && !next_target(-_last_dir, to)) return false;
    return !_region.load_beside(book, _x, _y, to.x, to.y, budget, max_bytes);
}

}
