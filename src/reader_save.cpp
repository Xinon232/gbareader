#include "reader_save.h"

#include <cstring>

#ifdef __DEVKITARM__
#include "bn_sram.h"
#endif

namespace reader {
namespace {
constexpr uint32_t MAGIC = 0x31524247; // "GBR1"
constexpr uint16_t VERSION = 3;
constexpr int LEGACY_V1_FILENAME_MAX = 56;
constexpr int LEGACY_V1_WIRE_SIZE = 80;
constexpr int LEGACY_V2_FILENAME_MAX = 64;
constexpr int LEGACY_V2_WIRE_SIZE = 84;
constexpr int LAST_FILENAME_OFFSET = 12;
constexpr int POSITION_RECORD_SIZE = 4 + SAVE_FILENAME_MAX;
constexpr int POSITIONS_OFFSET = LAST_FILENAME_OFFSET + SAVE_FILENAME_MAX;
constexpr int CHECKSUM_OFFSET = POSITIONS_OFFSET + SAVE_POSITION_SLOTS * POSITION_RECORD_SIZE;
static_assert(SAVE_WIRE_SIZE == CHECKSUM_OFFSET + 4);

#ifdef __DEVKITARM__
__attribute__((section(".sbss")))
#endif
unsigned char save_wire[SAVE_WIRE_SIZE]{};

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

bool valid_filename(const char* filename)
{
    if(! filename || ! filename[0]) return false;
    for(int index = 0; index < SAVE_FILENAME_MAX; ++index) {
        if(! filename[index]) return true;
    }
    return false;
}

void copy_filename(char* destination, const char* source)
{
    int index = 0;
    if(source) {
        while(index < SAVE_FILENAME_MAX - 1 && source[index]) {
            destination[index] = source[index];
            ++index;
        }
    }
    destination[index] = 0;
    while(++index < SAVE_FILENAME_MAX) destination[index] = 0;
}

bool same_filename(const char* first, const char* second)
{
    if(! first || ! second) return false;
    for(int index = 0; index < SAVE_FILENAME_MAX; ++index) {
        if(first[index] != second[index]) return false;
        if(! first[index]) return true;
    }
    return false;
}

bool deserialize_legacy(const unsigned char* input, size_t size, int version, SaveData& save)
{
    const int filename_size = version == 1 ? LEGACY_V1_FILENAME_MAX : LEGACY_V2_FILENAME_MAX;
    const int wire_size = version == 1 ? LEGACY_V1_WIRE_SIZE : LEGACY_V2_WIRE_SIZE;
    const int checksum_offset = wire_size - 4;
    if(size < size_t(wire_size) || get32(input + checksum_offset) != checksum(input, checksum_offset)) return false;

    save = default_save();
    save.settings = { input[8], input[9], input[10] };
    clamp_settings(save.settings);
    char filename[SAVE_FILENAME_MAX]{};
    std::memcpy(filename, input + 16, filename_size);
    filename[filename_size - 1] = 0;
    copy_filename(save.last_filename, filename);
    if(filename[0]) {
        save.positions[0].byte_offset = get32(input + 12);
        copy_filename(save.positions[0].filename, filename);
    }
    return true;
}
}

SaveData default_save()
{
    SaveData result{};
    result.settings = default_settings();
    return result;
}

bool find_saved_position(const SaveData& save, const char* filename, uint32_t& byte_offset)
{
    byte_offset = 0;
    if(! valid_filename(filename)) return false;
    for(int index = 0; index < SAVE_POSITION_SLOTS; ++index) {
        if(same_filename(save.positions[index].filename, filename)) {
            byte_offset = save.positions[index].byte_offset;
            return true;
        }
    }
    return false;
}

void update_saved_position(SaveData& save, const char* filename, uint32_t byte_offset)
{
    if(! valid_filename(filename)) return;
    int slot = -1;
    for(int index = 0; index < SAVE_POSITION_SLOTS; ++index) {
        if(same_filename(save.positions[index].filename, filename)) {
            slot = index;
            break;
        }
        if(slot < 0 && ! save.positions[index].filename[0]) slot = index;
    }
    if(slot < 0) {
        slot = save.next_replacement % SAVE_POSITION_SLOTS;
        save.next_replacement = uint8_t((slot + 1) % SAVE_POSITION_SLOTS);
    }
    save.positions[slot].byte_offset = byte_offset;
    copy_filename(save.positions[slot].filename, filename);
}

bool serialize_save(const SaveData& save, unsigned char* output, size_t size)
{
    if(! output || size < SAVE_WIRE_SIZE) return false;
    std::memset(output, 0, SAVE_WIRE_SIZE);
    put32(output, MAGIC);
    output[4] = VERSION & 0xFF;
    output[5] = VERSION >> 8;
    output[6] = save.next_replacement % SAVE_POSITION_SLOTS;
    Settings settings = save.settings;
    clamp_settings(settings);
    output[8] = settings.line_spacing;
    output[9] = settings.top_margin;
    output[10] = settings.bottom_margin;
    copy_filename(reinterpret_cast<char*>(output + LAST_FILENAME_OFFSET), save.last_filename);
    for(int index = 0; index < SAVE_POSITION_SLOTS; ++index) {
        unsigned char* record = output + POSITIONS_OFFSET + index * POSITION_RECORD_SIZE;
        put32(record, save.positions[index].byte_offset);
        copy_filename(reinterpret_cast<char*>(record + 4), save.positions[index].filename);
    }
    put32(output + CHECKSUM_OFFSET, checksum(output, CHECKSUM_OFFSET));
    return true;
}

bool deserialize_save(const unsigned char* input, size_t size, SaveData& save)
{
    if(! input || size < LEGACY_V1_WIRE_SIZE || get32(input) != MAGIC || input[5] != 0) return false;
    const int version = input[4];
    if(version == 1 || version == 2) return deserialize_legacy(input, size, version, save);
    if(version != VERSION || size < SAVE_WIRE_SIZE ||
       get32(input + CHECKSUM_OFFSET) != checksum(input, CHECKSUM_OFFSET)) return false;

    save = {};
    save.settings = { input[8], input[9], input[10] };
    clamp_settings(save.settings);
    save.next_replacement = input[6] % SAVE_POSITION_SLOTS;
    std::memcpy(save.last_filename, input + LAST_FILENAME_OFFSET, SAVE_FILENAME_MAX);
    save.last_filename[SAVE_FILENAME_MAX - 1] = 0;
    for(int index = 0; index < SAVE_POSITION_SLOTS; ++index) {
        const unsigned char* record = input + POSITIONS_OFFSET + index * POSITION_RECORD_SIZE;
        save.positions[index].byte_offset = get32(record);
        std::memcpy(save.positions[index].filename, record + 4, SAVE_FILENAME_MAX);
        save.positions[index].filename[SAVE_FILENAME_MAX - 1] = 0;
    }
    return true;
}

bool load_save(SaveData& save)
{
#ifdef __DEVKITARM__
    bn::sram::read_offset(save_wire, 0);
#endif
    return deserialize_save(save_wire, sizeof(save_wire), save);
}

void store_save(const SaveData& save)
{
    serialize_save(save, save_wire, sizeof(save_wire));
#ifdef __DEVKITARM__
    bn::sram::write_offset(save_wire, 0);
#endif
}

}
