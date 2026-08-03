#pragma once

#include "reader_file.h"

#include <cstddef>
#include <cstdint>

namespace reader {

constexpr int SAVE_FILENAME_MAX = LIBRARY_NAME_MAX;
constexpr int SAVE_POSITION_SLOTS = 32;
constexpr int SAVE_WIRE_SIZE = 12 + SAVE_FILENAME_MAX +
                               SAVE_POSITION_SLOTS * (4 + SAVE_FILENAME_MAX) + 4;

struct SavedPosition {
    uint32_t byte_offset;
    char filename[SAVE_FILENAME_MAX];
};

struct SaveData {
    Settings settings;
    uint8_t next_replacement;
    char last_filename[SAVE_FILENAME_MAX];
    SavedPosition positions[SAVE_POSITION_SLOTS];
};

SaveData default_save();
bool find_saved_position(const SaveData& save, const char* filename, uint32_t& byte_offset);
void update_saved_position(SaveData& save, const char* filename, uint32_t byte_offset);
bool serialize_save(const SaveData& save, unsigned char* output, size_t size);
bool deserialize_save(const unsigned char* input, size_t size, SaveData& save);
bool load_save(SaveData& save);
void store_save(const SaveData& save);

}
