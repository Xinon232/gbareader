#pragma once

#include "reader_core.h"
#include "reader_txt_save.h"

#include <cstdint>

#ifdef __DEVKITARM__
#include "ff.h"
#endif

namespace reader {

constexpr int LIBRARY_MAX_FILES = 64;
constexpr int LIBRARY_NAME_MAX = 256;
constexpr int EPUB_CACHE_TRAILER_SIZE = 32;

struct EpubCacheInfo {
    uint32_t cache_start;
    uint32_t text_size;
    uint32_t book_size;
    uint32_t text_crc32;
};

void make_epub_cache_trailer(uint32_t cache_start, uint32_t text_size,
                             uint32_t book_size, uint32_t text_crc32,
                             unsigned char output[EPUB_CACHE_TRAILER_SIZE]);
bool parse_epub_cache_trailer(const unsigned char* input, int size,
                              uint32_t payload_size, EpubCacheInfo& info);
bool validate_epub_cache_payload(const ByteSource& source, uint32_t cache_start,
                                 uint32_t text_size, uint32_t expected_crc32);

struct BookStorageLayout {
    uint32_t book_size;
    uint32_t footer_offset;
    uint32_t footer_size;
    uint32_t cache_start;
    uint32_t cache_size;
    uint32_t cache_crc32;
    bool has_valid_footer;
    bool has_valid_cache;
};

bool inspect_book_tail(const char* name, uint32_t physical_size,
                       const unsigned char* tail, uint32_t tail_size,
                       BookStorageLayout& layout);

bool storage_init();
int library_count();
const char* library_name(int index);
bool supported_book_name(const char* name);
bool txt_book_name(const char* name);
bool book_size_without_footer(const char* name, uint32_t physical_size,
                              const unsigned char* tail, uint32_t tail_size,
                              uint32_t& logical_size, bool& has_valid_footer,
                              uint32_t& footer_size);
const char* save_result_string(bool saved);

#ifndef __DEVKITARM__
struct FooterWriteTestResult {
    uint32_t physical_size;
    bool success;
    bool old_footer_restored;
};
FooterWriteTestResult footer_write_transaction_for_tests(uint32_t existing_footer_size,
                                                         int first_write_limit,
                                                         bool first_sync_fails);
struct EpubCacheWriteTestResult {
    uint32_t physical_size;
    bool success;
    bool old_footer_restored;
};
EpubCacheWriteTestResult epub_cache_write_transaction_for_tests(
        uint32_t text_size, uint32_t existing_footer_size,
        int failed_write, bool first_sync_fails);
#endif

class ReaderFile final : public ByteSource {
public:
    ReaderFile();
    ~ReaderFile() override;
    bool open_read_only(const char* filename);
    void close();
    bool is_open() const { return _open; }
    uint32_t size() const override { return _size; }
    bool byte_at(uint32_t offset, unsigned char& value) const override;
    uint32_t optimized_size() const override { return _has_valid_cache ? _epub_cache_size : 0; }
    bool optimized_byte_at(uint32_t offset, unsigned char& value) const override;
    bool saved_footer(TxtSaveFooter& footer) const;
    bool save_footer(const TxtSaveFooter& footer, const ByteSource* optimized_source = nullptr);

private:
#ifdef __DEVKITARM__
    mutable FIL _file;
#endif
    mutable uint32_t _cache_start;
    mutable int _cache_size;
    mutable unsigned char _cache[512];
    unsigned char _write_cache[512];
    unsigned char _previous_footer[TXT_SAVE_FOOTER_SIZE];
    uint32_t _size;
    uint32_t _physical_size;
    uint32_t _footer_size;
    uint32_t _footer_offset;
    uint32_t _epub_cache_start;
    uint32_t _epub_cache_size;
    bool _has_footer;
    bool _has_valid_cache;
    bool _open;
    char _name[LIBRARY_NAME_MAX];

    bool physical_byte_at(uint32_t offset, unsigned char& value) const;
};

}
