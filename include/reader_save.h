#pragma once

#include "reader_file.h"

#include <cstddef>
#include <cstdint>

namespace reader {

constexpr int SAVE_FILENAME_MAX = LIBRARY_NAME_MAX;
constexpr int SAVE_WIRE_SIZE = 84;

struct SaveData {
    Settings settings;
    uint32_t byte_offset;
    char filename[SAVE_FILENAME_MAX];
};

SaveData default_save();
bool serialize_save(const SaveData& save, unsigned char* output, size_t size);
bool deserialize_save(const unsigned char* input, size_t size, SaveData& save);
bool load_save(SaveData& save);
void store_save(const SaveData& save);

}
