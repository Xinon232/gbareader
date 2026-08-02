#include "reader_save.h"
#include "reader_file.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace reader;

static uint32_t legacy_checksum(const unsigned char* bytes, size_t size)
{
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static void put32(unsigned char* bytes, uint32_t value)
{
    for(int i = 0; i < 4; ++i) bytes[i] = static_cast<unsigned char>(value >> (i * 8));
}

int main()
{
    static_assert(SAVE_FILENAME_MAX == LIBRARY_NAME_MAX);
    SaveData input = default_save();
    input.settings.line_spacing = 3;
    input.settings.top_margin = 7;
    input.settings.bottom_margin = 9;
    input.byte_offset = 0x12345678;
    std::strcpy(input.filename, "BOOK.TXT");

    unsigned char bytes[SAVE_WIRE_SIZE]{};
    assert(serialize_save(input, bytes, sizeof(bytes)));
    SaveData output{};
    assert(deserialize_save(bytes, sizeof(bytes), output));
    assert(output.byte_offset == input.byte_offset);
    assert(output.settings.line_spacing == 3);
    assert(std::strcmp(output.filename, "BOOK.TXT") == 0);

    SaveData longest = default_save();
    for(int i = 0; i < LIBRARY_NAME_MAX - 1; ++i) longest.filename[i] = char('a' + i % 26);
    longest.filename[LIBRARY_NAME_MAX - 5] = '.';
    longest.filename[LIBRARY_NAME_MAX - 4] = 't';
    longest.filename[LIBRARY_NAME_MAX - 3] = 'x';
    longest.filename[LIBRARY_NAME_MAX - 2] = 't';
    longest.filename[LIBRARY_NAME_MAX - 1] = 0;
    assert(serialize_save(longest, bytes, sizeof(bytes)));
    assert(bytes[4] == 2 && bytes[5] == 0);
    assert(deserialize_save(bytes, sizeof(bytes), output));
    assert(std::strcmp(output.filename, longest.filename) == 0);

    bytes[17] ^= 0x40;
    assert(! deserialize_save(bytes, sizeof(bytes), output));

    unsigned char legacy[80]{};
    put32(legacy, 0x31524247);
    legacy[4] = 1;
    legacy[8] = 2;
    legacy[9] = 8;
    legacy[10] = 8;
    put32(legacy + 12, 1234);
    std::strcpy(reinterpret_cast<char*>(legacy + 16), "OLD.TXT");
    put32(legacy + 76, legacy_checksum(legacy, 76));
    assert(deserialize_save(legacy, sizeof(legacy), output));
    assert(output.byte_offset == 1234);
    assert(std::strcmp(output.filename, "OLD.TXT") == 0);
    std::puts("PASS: reader save checksum");
}
