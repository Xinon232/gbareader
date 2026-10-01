#pragma once
#include "reader_core.h"
#ifdef __DEVKITARM__
#include "ff.h"
#endif

namespace reader {
// V2: 32 fixed bytes + 0..255 exact UTF-8 filename bytes (no terminator on disk).
constexpr uint32_t GLOBAL_SETTINGS_BYTES = 32;
constexpr int GLOBAL_BOOK_NAME_MAX = 256;
constexpr uint32_t GLOBAL_SETTINGS_MAX_BYTES = GLOBAL_SETTINGS_BYTES + GLOBAL_BOOK_NAME_MAX - 1;
struct GlobalPreferences {
    uint8_t line_spacing = 1;
    ParagraphGap paragraph_gap = ParagraphGap::FULL;
    bool shoulder_page_turns = false; // L/R turn pages; the last choice, restored at launch
    bool show_extensions = true; // Home shows ".txt" / ".epub"; stored inverted (0 = shown)
    char last_book[GLOBAL_BOOK_NAME_MAX]{};
};
bool remember_global_book(GlobalPreferences&, const char* filename);
int remembered_library_selection(const char* filename, int count, const char* (*name_at)(int));
const char* save_status_message(bool position_saved, bool settings_saved);
uint32_t global_settings_size(const GlobalPreferences&);
GlobalPreferences default_global_preferences();
bool same_global_preferences(const GlobalPreferences&, const GlobalPreferences&);
bool encode_global_settings(const GlobalPreferences&, uint32_t generation, unsigned char* output);
bool decode_global_settings(const unsigned char*, uint32_t length, GlobalPreferences&, uint32_t& generation);
#ifdef __DEVKITARM__
enum class GlobalLoadResult { MISSING, LOADED, RECOVERED, ERROR };
class GlobalSettingsStore {
public:
    GlobalPreferences values{};
    GlobalSettingsStore();
    ~GlobalSettingsStore() { close_handle(); }
    GlobalSettingsStore(const GlobalSettingsStore&) = delete;
    GlobalSettingsStore& operator=(const GlobalSettingsStore&) = delete;
    GlobalLoadResult load();
    bool dirty() const { return !_basis_known || _uncertain || !same_global_preferences(values, _persisted); }
    bool save();
    uint32_t generation() const { return _generation; }
    int active_slot() const { return _active; }
private:
    enum class Slot { MISSING, VALID, DAMAGED, COLLISION, ERROR };
    bool close_handle();
    GlobalLoadResult read_basis();
    Slot read_slot(int slot, GlobalPreferences&, uint32_t&);
    GlobalPreferences _persisted{};
    FIL _file{};
    unsigned char _bytes[GLOBAL_SETTINGS_MAX_BYTES]{};
    uint32_t _generation = 0;
    int _active = -1;
    bool _open = false;
    bool _basis_known = false; // Both slots must be accounted for before a save.
    bool _uncertain = false; // A failed write/readback may already be durable.
    bool _created[2]{}; // Only this object's successful CREATE_NEW grants empty ownership.
};
#endif
}
