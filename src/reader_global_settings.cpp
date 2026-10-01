#include "reader_global_settings.h"
#include "reader_crc32.h"
#include <cstring>

namespace reader {
namespace {
constexpr char MAGIC[] = "GBARCFG1";
uint32_t get32(const unsigned char* b) {
    return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}
void put32(unsigned char* b, uint32_t v) {
    for(int i = 0; i < 4; ++i) b[i] = uint8_t(v >> (8 * i));
}
int name_length(const char* name) {
    if(!name) return -1;
    for(int i = 0; i < GLOBAL_BOOK_NAME_MAX; ++i) {
        if(!name[i]) return i;
        if(uint8_t(name[i]) < 32 || name[i] == '/' || name[i] == '\\') return -1;
    }
    return -1;
}
bool valid_preferences(const GlobalPreferences& p) {
    return p.line_spacing <= MAX_LINE_SPACING && unsigned(p.paragraph_gap) < PARAGRAPH_GAP_COUNT &&
            name_length(p.last_book) >= 0;
}
}
bool remember_global_book(GlobalPreferences& p, const char* filename) {
    const int length = name_length(filename);
    if(length < 0) return false;
    // memmove also permits callers to pass the current remembered name.
    memmove(p.last_book, filename, unsigned(length) + 1);
    memset(p.last_book + length + 1, 0, GLOBAL_BOOK_NAME_MAX - unsigned(length) - 1);
    return true;
}
int remembered_library_selection(const char* filename, int count, const char* (*name_at)(int)) {
    if(!filename || !filename[0]) return 0;
    for(int i = 0; i < count; ++i) {
        const char* name = name_at(i);
        if(!name) return 0; // A failed scan must not leave an out-of-range selection.
        if(!strcmp(filename, name)) return i;
    }
    return 0;
}
const char* save_status_message(bool position_saved, bool settings_saved) {
    if(!position_saved && !settings_saved) return "Both saves failed";
    if(!position_saved) return "Position save failed";
    return settings_saved ? "Saved" : "Settings save failed";
}
uint32_t global_settings_size(const GlobalPreferences& p) {
    const int length = name_length(p.last_book);
    return length < 0 ? 0 : GLOBAL_SETTINGS_BYTES + unsigned(length);
}
GlobalPreferences default_global_preferences() { return {}; }
bool same_global_preferences(const GlobalPreferences& a, const GlobalPreferences& b) {
    return a.line_spacing == b.line_spacing && a.paragraph_gap == b.paragraph_gap &&
            a.shoulder_page_turns == b.shoulder_page_turns && !strcmp(a.last_book, b.last_book);
}
bool encode_global_settings(const GlobalPreferences& p, uint32_t generation, unsigned char* b) {
    if(!generation || !valid_preferences(p)) return false;
    const uint32_t length = global_settings_size(p);
    memset(b, 0, length);
    memcpy(b, MAGIC, 8);
    put32(b + 8, 2); put32(b + 12, generation);
    b[16] = p.line_spacing; b[17] = uint8_t(p.paragraph_gap); b[18] = p.shoulder_page_turns;
    b[19] = uint8_t(length - GLOBAL_SETTINGS_BYTES);
    memcpy(b + 28, p.last_book, b[19]);
    put32(b + length - 4, crc32_bytes(b, length - 4));
    return true;
}
bool decode_global_settings(const unsigned char* b, uint32_t length, GlobalPreferences& p, uint32_t& generation) {
    if(length < GLOBAL_SETTINGS_BYTES || length > GLOBAL_SETTINGS_MAX_BYTES || memcmp(b, MAGIC, 8)) return false;
    const uint32_t version = get32(b + 8);
    if(version != 1 && version != 2) return false;
    const unsigned name_bytes = version == 2 ? b[19] : 0;
    if(length != GLOBAL_SETTINGS_BYTES + name_bytes || !get32(b + 12) ||
       b[16] > MAX_LINE_SPACING || b[17] >= PARAGRAPH_GAP_COUNT || b[18] > 1 ||
       get32(b + length - 4) != crc32_bytes(b, length - 4)) return false;
    for(int i = version == 1 ? 19 : 20; i < 28; ++i) if(b[i]) return false;
    for(unsigned i = 0; i < name_bytes; ++i)
        if(b[28 + i] < 32 || b[28 + i] == '/' || b[28 + i] == '\\') return false;
    p = {b[16], ParagraphGap(b[17]), bool(b[18]), {}};
    memcpy(p.last_book, b + 28, name_bytes);
    generation = get32(b + 12);
    return true;
}

#ifdef __DEVKITARM__
namespace {
const char* const paths[] = {"/gbareader/SETTINGS0.DAT", "/gbareader/SETTINGS1.DAT"};
}
GlobalSettingsStore::GlobalSettingsStore() {}
bool GlobalSettingsStore::close_handle() {
    if(_open) {
        if(f_close(&_file) != FR_OK) return false;
        _open = false;
    }
    return true;
}
GlobalSettingsStore::Slot GlobalSettingsStore::read_slot(int slot, GlobalPreferences& p, uint32_t& generation) {
    if(!close_handle()) return Slot::ERROR;
    const FRESULT opened = f_open(&_file, paths[slot], FA_READ | FA_OPEN_EXISTING);
    if(opened == FR_NO_FILE) return Slot::MISSING;
    if(opened != FR_OK) return Slot::ERROR;
    _open = true;
    const uint32_t length = uint32_t(f_size(&_file));
    const UINT count = length < GLOBAL_SETTINGS_MAX_BYTES ? length : GLOBAL_SETTINGS_MAX_BYTES;
    UINT got = 0;
    const bool read = f_read(&_file, _bytes, count, &got) == FR_OK && got == count;
    if(!close_handle() || !read) return Slot::ERROR;
    if(decode_global_settings(_bytes, length, p, generation)) return Slot::VALID;
    // Never infer ownership from a filename or an old empty creation artifact.
    if(length >= 12 && !memcmp(_bytes, MAGIC, 8) && (get32(_bytes + 8) == 1 || get32(_bytes + 8) == 2))
        return Slot::DAMAGED;
    return Slot::COLLISION;
}
GlobalLoadResult GlobalSettingsStore::read_basis() {
    // Discover disk authority without replacing pending RAM preferences/name.
    _persisted = {}; _generation = 0; _active = -1; _basis_known = true;
    bool problem = false, missing = true;
    GlobalPreferences candidate;
    for(int slot = 0; slot < 2; ++slot) {
        uint32_t generation = 0;
        const Slot state = read_slot(slot, candidate, generation);
        if(state == Slot::ERROR) _basis_known = false;
        missing = missing && state == Slot::MISSING;
        problem = problem || (state != Slot::MISSING && state != Slot::VALID);
        // Equal generations deterministically prefer slot zero.
        if(state == Slot::VALID && generation > _generation) {
            _persisted = candidate; _generation = generation; _active = slot;
        }
    }
    if(_active >= 0) return problem ? GlobalLoadResult::RECOVERED : GlobalLoadResult::LOADED;
    return missing ? GlobalLoadResult::MISSING : GlobalLoadResult::ERROR;
}
GlobalLoadResult GlobalSettingsStore::load() {
    if(!close_handle()) { _basis_known = false; return GlobalLoadResult::ERROR; }
    _uncertain = false;
    const GlobalLoadResult result = read_basis();
    values = _persisted;
    return result;
}
bool GlobalSettingsStore::save() {
    if(!close_handle()) return false;
    // Only failed/incomplete loads (or save-before-load) need another scan.
    // An unreadable partner is not absence: it may hold a newer generation.
    if(!_basis_known) {
        read_basis();
        if(!_basis_known) return false;
    }
    if(!dirty()) return true;
    if(_generation == 0xffffffffu || !valid_preferences(values)) return false;
    const int target = _active == 0 ? 1 : 0;
    GlobalPreferences candidate;
    uint32_t generation = 0;
    const Slot state = read_slot(target, candidate, generation);
    if(state == Slot::ERROR ||
       (state == Slot::COLLISION && !_created[target]) ||
       (state == Slot::DAMAGED && _active < 0 && !_created[target])) return false;
    const BYTE mode = FA_WRITE | (state == Slot::MISSING ? FA_CREATE_NEW : FA_OPEN_EXISTING);
    if(f_open(&_file, paths[target], mode) != FR_OK) return false;
    _open = true;
    if(state == Slot::MISSING) _created[target] = true;
    const uint32_t length = global_settings_size(values);
    const uint32_t next = _generation + 1;
    encode_global_settings(values, next, _bytes);
    _uncertain = true;
    UINT written = 0;
    const bool written_ok = f_lseek(&_file, 0) == FR_OK &&
            f_write(&_file, _bytes, length, &written) == FR_OK && written == length &&
            f_truncate(&_file) == FR_OK && f_sync(&_file) == FR_OK;
    const bool closed = close_handle();
    if(!written_ok || !closed) return false;
    if(f_open(&_file, paths[target], FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;
    _open = true;
    bool exact = f_size(&_file) == length;
    unsigned char chunk[32];
    for(uint32_t at = 0; exact && at < length; at += sizeof(chunk)) {
        const UINT n = length - at < sizeof(chunk) ? length - at : sizeof(chunk);
        UINT got = 0;
        exact = f_read(&_file, chunk, n, &got) == FR_OK && got == n && !memcmp(chunk, _bytes + at, n);
    }
    if(!close_handle() || !exact) return false;
    _persisted = values; _generation = next; _active = target;
    _created[target] = false; _uncertain = false;
    return true;
}
#endif
}
