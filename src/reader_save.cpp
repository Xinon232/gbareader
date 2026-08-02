#include "reader_save.h"

#include <cstring>

#ifdef __DEVKITARM__
#include "bn_sram.h"
#endif

namespace reader {
namespace {
constexpr uint32_t MAGIC = 0x31524247; // "GBR1"
constexpr uint16_t VERSION = 2;
constexpr int LEGACY_FILENAME_MAX = 56;
constexpr int LEGACY_WIRE_SIZE = 80;

void put32(unsigned char* p, uint32_t value)
{
    for(int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (i * 8));
}

uint32_t get32(const unsigned char* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint32_t checksum(const unsigned char* p, size_t size)
{
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < size; ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}

#ifndef __DEVKITARM__
unsigned char host_sram[SAVE_WIRE_SIZE]{};
#endif
}

SaveData default_save()
{
    SaveData result{};
    result.settings = default_settings();
    return result;
}

bool serialize_save(const SaveData& save, unsigned char* output, size_t size)
{
    if(! output || size < SAVE_WIRE_SIZE) return false;
    std::memset(output, 0, SAVE_WIRE_SIZE);
    put32(output, MAGIC);
    output[4] = VERSION & 0xFF;
    output[5] = VERSION >> 8;
    Settings settings = save.settings;
    clamp_settings(settings);
    output[8] = settings.line_spacing;
    output[9] = settings.top_margin;
    output[10] = settings.bottom_margin;
    put32(output + 12, save.byte_offset);
    std::memcpy(output + 16, save.filename, SAVE_FILENAME_MAX);
    output[16 + SAVE_FILENAME_MAX - 1] = 0;
    put32(output + 80, checksum(output, 80));
    return true;
}

bool deserialize_save(const unsigned char* input, size_t size, SaveData& save)
{
    if(! input || size < LEGACY_WIRE_SIZE || get32(input) != MAGIC || input[5] != 0) return false;
    const int version = input[4];
    const int filename_size = version == 1 ? LEGACY_FILENAME_MAX :
                              version == VERSION ? SAVE_FILENAME_MAX : 0;
    const int checksum_offset = version == 1 ? 76 : 80;
    if(! filename_size || size < size_t(checksum_offset + 4) ||
       get32(input + checksum_offset) != checksum(input, checksum_offset)) return false;
    save = {};
    save.settings = { input[8], input[9], input[10] };
    clamp_settings(save.settings);
    save.byte_offset = get32(input + 12);
    std::memcpy(save.filename, input + 16, filename_size);
    save.filename[SAVE_FILENAME_MAX - 1] = 0;
    return true;
}

bool load_save(SaveData& save)
{
    unsigned char bytes[SAVE_WIRE_SIZE];
#ifdef __DEVKITARM__
    bn::sram::read_offset(bytes, 0);
#else
    std::memcpy(bytes, host_sram, sizeof(bytes));
#endif
    return deserialize_save(bytes, sizeof(bytes), save);
}

void store_save(const SaveData& save)
{
    unsigned char bytes[SAVE_WIRE_SIZE];
    serialize_save(save, bytes, sizeof(bytes));
#ifdef __DEVKITARM__
    bn::sram::write_offset(bytes, 0);
#else
    std::memcpy(host_sram, bytes, sizeof(bytes));
#endif
}

}
