#pragma once

#include "reader_core.h"
#include "miniz.h"

#include <cstdint>

namespace reader {

constexpr int EPUB_MAX_SPINE_ITEMS = 256;
constexpr int EPUB_MAX_PATH = 256;
constexpr uint32_t EPUB_TEXT_WINDOW_BYTES = 16 * 1024;
constexpr uint32_t EPUB_INFLATE_DICTIONARY_BYTES = 32 * 1024;
constexpr uint32_t EPUB_MAX_XHTML_BYTES = 32 * 1024 * 1024;
constexpr uint32_t EPUB_MAX_METADATA_BYTES = 64 * 1024;
constexpr uint32_t EPUB_MAX_COMPRESSED_BYTES = 16 * 1024 * 1024;
constexpr uint32_t EPUB_MAX_ARCHIVE_BYTES = 128 * 1024 * 1024;

enum class EpubError : uint8_t {
    NONE, READ_FAILED, NOT_ZIP, MULTI_DISK, ZIP64, ENCRYPTED,
    UNSUPPORTED_COMPRESSION, MALFORMED_ZIP, ARCHIVE_TOO_LARGE,
    METADATA_TOO_LARGE, COMPRESSED_ENTRY_TOO_LARGE, CHAPTER_TOO_LARGE,
    TOO_MANY_SPINE_ITEMS, BOOK_TEXT_TOO_LARGE,
    MISSING_CONTAINER, MISSING_ROOTFILE, MISSING_MANIFEST_ITEM, MISSING_SPINE,
    UNSAFE_PATH, INVALID_XHTML
};

const char* epub_error_string(EpubError error);

class EpubDocument final : public ByteSource {
public:
    EpubDocument();
    bool open(const ByteSource& archive);
    void close();
    uint32_t size() const override { return _virtual_size; }
    bool byte_at(uint32_t offset, unsigned char& value) const override;
    EpubError error() const { return _error; }

private:
    struct ZipEntry {
        char name[EPUB_MAX_PATH];
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint32_t local_offset;
        uint32_t central_offset;
        uint32_t crc32;
        uint16_t method;
        uint16_t flags;
        uint16_t name_length;
    };
    struct SpineItem { uint32_t central_offset; uint32_t start; uint32_t size; };

    bool parse_zip();
    bool build_spine();
    bool load_entry(const ZipEntry& entry, uint32_t uncompressed_limit) const;
    bool stream_chapter(int spine_index, uint32_t window_start, bool count_only) const;
    int find_entry(const char* name, ZipEntry& entry) const;
    bool read_entry(uint32_t central_offset, ZipEntry& entry) const;
    bool validate_local_entry(const ZipEntry& entry, uint32_t& data_offset) const;
    bool fail(EpubError error) const;

    const ByteSource* _archive;
    SpineItem _spine[EPUB_MAX_SPINE_ITEMS];
    uint32_t _central_offset;
    uint32_t _central_size;
    int _entry_count;
    int _spine_count;
    uint32_t _virtual_size;
    mutable EpubError _error;
    mutable int _cached_spine;
    mutable uint32_t _window_start;
    mutable uint32_t _window_size;
    mutable uint32_t _buffer_size;
    mutable tinfl_decompressor _inflator;
    mutable unsigned char _input[512];
    union Workspace {
        unsigned char metadata[EPUB_MAX_METADATA_BYTES];
        struct {
            unsigned char dictionary[EPUB_INFLATE_DICTIONARY_BYTES];
            unsigned char text[EPUB_TEXT_WINDOW_BYTES];
        } stream;
    };
    mutable Workspace _workspace;
};

}
