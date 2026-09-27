#pragma once

#include "reader_core.h"
#include "reader_txt_save.h"

#include <cstdint>

#ifdef __DEVKITARM__
#include "ff.h"
#else
#include <vector>
#endif

namespace reader {

constexpr int LIBRARY_MAX_FILES = 64;
constexpr int LIBRARY_NAME_MAX = 256;
constexpr int LIBRARY_PATH_MAX = LIBRARY_NAME_MAX + sizeof("/gbareader/") - 1;
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
bool scan_library();
bool library_path(int index, char (&path)[LIBRARY_PATH_MAX]);
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
class EpubDocument;
enum class EpubTestFault { READ, SEEK, WRITE, TRUNCATE, SYNC, NONE };
struct EpubAppendTestResult { bool success; bool fault_hit; int calls[5]; };
EpubAppendTestResult append_epub_transaction_for_tests(
        std::vector<unsigned char>& raw, const ByteSource* text, const TxtSaveFooter& footer,
        EpubTestFault fault = EpubTestFault::NONE, int nth = 0, uint32_t append_offset = 0);
bool write_epub_cache_file_for_tests(const char* input, const char* output,
                                     const EpubDocument& normalized);
bool corrupt_epub_cache_file_for_tests(const char* path);
struct FooterWriteTestResult {
    uint32_t physical_size;
    bool success;
    bool old_footer_restored;
    bool old_footer_parseable;
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
    bool read_ready() const override { return ensure_source(); }
    bool read_range(uint32_t offset, unsigned char* output, uint32_t count) const override;
    uint32_t optimized_size() const override { return _has_valid_cache ? _epub_cache_size : 0; }
    bool optimized_byte_at(uint32_t offset, unsigned char& value) const override;
    bool saved_footer(TxtSaveFooter& footer) const;
    bool save_footer(const TxtSaveFooter& footer, const ByteSource* optimized_source = nullptr);
    bool companion_present() const override { return _side_present; }
    bool companion_cache(uint32_t& bytes, uint32_t& crc) const override;
    bool companion_read(uint32_t offset, unsigned char* output, uint32_t count) const override;
    bool archive_fingerprint(uint32_t& fingerprint) const override;

private:
#ifdef __DEVKITARM__
    mutable FIL _file;
    mutable bool _source_owned = false;
    mutable bool _source_verify = false;
    mutable bool _source_checking = false;
    mutable bool _source_changed = false;
    // Replaces the old 800-byte rollback buffer; books are never writable.
    mutable FIL _side_file;
#endif
    mutable bool _side_open = false;
    bool _side_present = false;
    bool _side_created = false;
    bool _side_valid = false;
    uint32_t _source_identity = 0;
    // EPUB central-directory fingerprint folded into _source_identity.
    mutable uint32_t _archive_fingerprint = 0;
    mutable bool _has_archive_fingerprint = false;
    uint32_t _side_generation = 0;
    uint32_t _side_slot = 0;
    uint32_t _side_cache_bytes = 0;
    uint32_t _side_cache_crc = 0;
    bool side_name(char* output) const;
    bool source_identity(uint32_t& result) const;
    bool ensure_source() const;
    bool load_sidecar();
    bool read_side_slot(uint32_t slot) const;
    bool write_side_cache(const ByteSource& text, uint32_t fingerprint);
    bool recover_side_write(uint32_t original_size);
    mutable uint32_t _cache_start;
    mutable int _cache_size;
    // 8 KiB EWRAM read window amortizes FatFS sector traffic during EPUB parsing.
    static constexpr uint32_t FILE_WINDOW_BYTES = 8 * 1024;
    mutable unsigned char _cache[FILE_WINDOW_BYTES];
    // One sector for isolated metadata reads (see window_at).
    static constexpr uint32_t SMALL_WINDOW_BYTES = 512;
    mutable unsigned char _small[SMALL_WINDOW_BYTES];
    mutable uint32_t _small_start = 0, _small_size = 0;
    const unsigned char* window_at(uint32_t offset, uint32_t& available) const;
    unsigned char _write_cache[512];
    uint32_t _size;
    uint32_t _physical_size;
    uint32_t _footer_size;
    uint32_t _footer_offset;
    uint32_t _epub_cache_start;
    uint32_t _epub_cache_size;
    bool _has_footer;
    bool _has_valid_cache;
    bool _open;
    char _name[LIBRARY_PATH_MAX];

    bool physical_byte_at(uint32_t offset, unsigned char& value) const;
};

}
