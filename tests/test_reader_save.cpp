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

static void expect_position(const SaveData& save, const char* filename, uint32_t expected)
{
    uint32_t actual = 0;
    assert(find_saved_position(save, filename, actual));
    assert(actual == expected);
}

int main()
{
    static_assert(SAVE_FILENAME_MAX == LIBRARY_NAME_MAX);
    static_assert(SAVE_POSITION_SLOTS == 32);

    SaveData input = default_save();
    input.settings = { 3, 4, 1 };
    std::strcpy(input.last_filename, "BOOK.TXT");
    update_saved_position(input, "BOOK.TXT", 0x12345678);
    update_saved_position(input, "NOVEL.EPUB", 0x2345);
    expect_position(input, "BOOK.TXT", 0x12345678);
    expect_position(input, "NOVEL.EPUB", 0x2345);
    uint32_t missing = 99;
    assert(! find_saved_position(input, "OTHER.TXT", missing));
    assert(missing == 0);

    unsigned char bytes[SAVE_WIRE_SIZE]{};
    assert(serialize_save(input, bytes, sizeof(bytes)));
    assert(bytes[4] == 3 && bytes[5] == 0);
    SaveData output{};
    assert(deserialize_save(bytes, sizeof(bytes), output));
    assert(output.settings.line_spacing == 3);
    assert(output.settings.top_margin == 4);
    assert(output.settings.bottom_margin == 1);
    assert(std::strcmp(output.last_filename, "BOOK.TXT") == 0);
    expect_position(output, "BOOK.TXT", 0x12345678);
    expect_position(output, "NOVEL.EPUB", 0x2345);

    char longest[SAVE_FILENAME_MAX]{};
    for(int i = 0; i < SAVE_FILENAME_MAX - 6; ++i) longest[i] = char('a' + i % 26);
    std::strcpy(longest + SAVE_FILENAME_MAX - 6, ".epub");
    update_saved_position(input, longest, 777);
    std::strcpy(input.last_filename, longest);
    assert(serialize_save(input, bytes, sizeof(bytes)));
    assert(deserialize_save(bytes, sizeof(bytes), output));
    assert(std::strcmp(output.last_filename, longest) == 0);
    expect_position(output, longest, 777);

    bytes[100] ^= 0x40;
    assert(! deserialize_save(bytes, sizeof(bytes), output));

    unsigned char version2[84]{};
    put32(version2, 0x31524247);
    version2[4] = 2;
    version2[8] = 2;
    version2[9] = 8;
    version2[10] = 0;
    put32(version2 + 12, 1234);
    std::strcpy(reinterpret_cast<char*>(version2 + 16), "OLD-V2.EPUB");
    put32(version2 + 80, legacy_checksum(version2, 80));
    assert(deserialize_save(version2, sizeof(version2), output));
    assert(output.settings.line_spacing == 2);
    assert(output.settings.top_margin == 4);
    assert(output.settings.bottom_margin == 1);
    assert(std::strcmp(output.last_filename, "OLD-V2.EPUB") == 0);
    expect_position(output, "OLD-V2.EPUB", 1234);

    unsigned char version1[80]{};
    put32(version1, 0x31524247);
    version1[4] = 1;
    version1[8] = 2;
    version1[9] = 3;
    version1[10] = 4;
    put32(version1 + 12, 4321);
    std::strcpy(reinterpret_cast<char*>(version1 + 16), "OLD-V1.TXT");
    put32(version1 + 76, legacy_checksum(version1, 76));
    assert(deserialize_save(version1, sizeof(version1), output));
    assert(std::strcmp(output.last_filename, "OLD-V1.TXT") == 0);
    expect_position(output, "OLD-V1.TXT", 4321);

    SaveData full = default_save();
    char name[32];
    for(int index = 0; index < SAVE_POSITION_SLOTS + 1; ++index) {
        std::snprintf(name, sizeof(name), "book-%02d.txt", index);
        update_saved_position(full, name, uint32_t(index * 10));
    }
    expect_position(full, "book-32.txt", 320);

    store_save(input);
    SaveData loaded = default_save();
    assert(load_save(loaded));
    expect_position(loaded, "BOOK.TXT", 0x12345678);
    expect_position(loaded, longest, 777);

    std::puts("PASS: reader save checksum and per-book positions");
}
