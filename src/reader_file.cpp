#include "reader_file.h"

#include <cstring>

#ifdef __DEVKITARM__
#include "bn_core.h"
#include "gbahw.h"
extern "C" {
#include "supercard_driver.h"
}
#endif

namespace reader {
namespace {

#ifdef __DEVKITARM__
__attribute__((section(".sbss")))
#endif
char names[LIBRARY_MAX_FILES][LIBRARY_NAME_MAX];
int name_count;
#ifdef __DEVKITARM__
FATFS fatfs;
#endif

bool extension_equal(const char* value, const char* extension)
{
    while(*value && *extension) {
        char left = *value++;
        char right = *extension++;
        if(left >= 'A' && left <= 'Z') left = char(left + ('a' - 'A'));
        if(left != right) return false;
    }
    return !*value && !*extension;
}

constexpr unsigned char EPUB_CACHE_MAGIC[8] = {'G','B','A','R','C','H','E','1'};
constexpr int CACHE_SIZE_AT = 8;
constexpr int CACHE_START_AT = 12;
constexpr int CACHE_TEXT_AT = 16;
constexpr int CACHE_BOOK_AT = 20;
constexpr int CACHE_TEXT_CRC_AT = 24;
constexpr int CACHE_CHECKSUM_AT = 28;

void write_u32(unsigned char* output, uint32_t value)
{
    output[0] = static_cast<unsigned char>(value);
    output[1] = static_cast<unsigned char>(value >> 8);
    output[2] = static_cast<unsigned char>(value >> 16);
    output[3] = static_cast<unsigned char>(value >> 24);
}

void write_u16(unsigned char* output, uint16_t value)
{
    output[0] = static_cast<unsigned char>(value);
    output[1] = static_cast<unsigned char>(value >> 8);
}

uint32_t read_u32(const unsigned char* input)
{
    return uint32_t(input[0]) | (uint32_t(input[1]) << 8) |
           (uint32_t(input[2]) << 16) | (uint32_t(input[3]) << 24);
}

uint32_t cache_metadata_hash(const unsigned char* input)
{
    uint32_t value = 2166136261u;
    for(int index = 0; index < EPUB_CACHE_TRAILER_SIZE; ++index) {
        if(index < CACHE_CHECKSUM_AT || index >= CACHE_CHECKSUM_AT + 4)
            value = (value ^ input[index]) * 16777619u;
    }
    return value;
}

uint32_t cache_crc32_byte(uint32_t crc, unsigned char value)
{
    static constexpr uint32_t table[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    crc ^= value;
    crc = (crc >> 4) ^ table[crc & 0x0Fu];
    return (crc >> 4) ^ table[crc & 0x0Fu];
}

template<typename Ops>
bool write_footer_transaction(Ops& ops, uint32_t logical_size,
                              const unsigned char replacement[TXT_SAVE_FOOTER_SIZE],
                              const unsigned char* previous, uint32_t previous_size)
{
    if(!ops.seek(logical_size)) return false;
    if(ops.write(replacement, TXT_SAVE_FOOTER_SIZE) && ops.truncate() && ops.sync()) return true;

    bool recovered = ops.seek(logical_size);
    if(recovered && previous_size) recovered = ops.write(previous, previous_size);
    if(recovered) recovered = ops.truncate();
    if(recovered) recovered = ops.sync();
    (void) recovered;
    return false;
}

template<typename Ops>
bool write_epub_cache_transaction(Ops& ops, uint32_t archive_size,
                                  const ByteSource& archive, uint32_t central_offset,
                                  uint32_t central_size, uint16_t entry_count,
                                  const ByteSource& text,
                                  const unsigned char replacement[TXT_SAVE_FOOTER_SIZE],
                                  const unsigned char* previous, uint32_t previous_size,
                                  unsigned char* scratch, uint32_t scratch_size)
{
    constexpr uint32_t EOCD_SIZE = 22;
    const uint64_t total = uint64_t(archive_size) + text.size() + central_size + EOCD_SIZE +
                           EPUB_CACHE_TRAILER_SIZE + TXT_SAVE_FOOTER_SIZE;
    if(!text.size() || !scratch_size || !entry_count || total > 0xFFFFFFFFu ||
       central_offset > archive.size() || central_size > archive.size() - central_offset)
        return false;

    uint32_t crc = 0xFFFFFFFFu;
    uint32_t copied = 0;
    bool written = true;
    while(written && copied < text.size()) {
        uint32_t count = text.size() - copied;
        if(count > scratch_size) count = scratch_size;
        for(uint32_t index = 0; index < count; ++index) {
            if(!text.byte_at(copied + index, scratch[index])) { written = false; break; }
            crc = cache_crc32_byte(crc, scratch[index]);
        }
        if(written) {
            written = ops.seek(archive_size + copied) && ops.write(scratch, count);
            copied += count;
        }
    }

    const uint32_t new_central_offset = archive_size + text.size();
    copied = 0;
    while(written && copied < central_size) {
        uint32_t count = central_size - copied;
        if(count > scratch_size) count = scratch_size;
        for(uint32_t index = 0; index < count; ++index) {
            if(!archive.byte_at(central_offset + copied + index, scratch[index])) {
                written = false;
                break;
            }
        }
        if(written) {
            written = ops.seek(new_central_offset + copied) && ops.write(scratch, count);
            copied += count;
        }
    }

    unsigned char eocd[EOCD_SIZE]{};
    write_u32(eocd, 0x06054B50u);
    write_u16(eocd + 8, entry_count);
    write_u16(eocd + 10, entry_count);
    write_u32(eocd + 12, central_size);
    write_u32(eocd + 16, new_central_offset);
    const uint32_t book_size = new_central_offset + central_size + EOCD_SIZE;
    if(written) written = ops.seek(new_central_offset + central_size) &&
                          ops.write(eocd, sizeof(eocd));

    unsigned char trailer[EPUB_CACHE_TRAILER_SIZE];
    make_epub_cache_trailer(archive_size, text.size(), book_size, ~crc, trailer);
    if(written) written = ops.write(trailer, EPUB_CACHE_TRAILER_SIZE);
    if(written) written = ops.write(replacement, TXT_SAVE_FOOTER_SIZE) &&
                          ops.truncate() && ops.sync();
    if(written) return true;

    bool recovered = ops.seek(archive_size);
    if(recovered && previous_size) recovered = ops.write(previous, previous_size);
    if(recovered) recovered = ops.truncate();
    if(recovered) recovered = ops.sync();
    (void) recovered;
    return false;
}

#ifdef __DEVKITARM__
struct FatFooterOps {
    FIL& file;
    bool seek(uint32_t offset) { return f_lseek(&file, offset) == FR_OK; }
    bool write(const unsigned char* data, uint32_t size)
    {
        UINT written = 0;
        return f_write(&file, data, size, &written) == FR_OK && written == size;
    }
    bool sync() { return f_sync(&file) == FR_OK; }
    bool truncate() { return f_truncate(&file) == FR_OK; }
};
#else
struct MemoryFooterOps {
    unsigned char data[1024]{};
    uint32_t size = 8;
    uint32_t position = 0;
    int first_write_limit = TXT_SAVE_FOOTER_SIZE;
    bool first_sync_fails = false;
    int failed_write = -1;
    int writes = 0;
    int syncs = 0;

    bool seek(uint32_t offset)
    {
        if(offset > size) return false;
        position = offset;
        return true;
    }

    bool write(const unsigned char* input, uint32_t count)
    {
        uint32_t amount = count;
        if(writes++ == 0 && first_write_limit < int(count)) amount = uint32_t(first_write_limit);
        if(writes - 1 == failed_write) amount = count ? count / 2 : 0;
        if(position + amount > sizeof(data)) return false;
        std::memcpy(data + position, input, amount);
        position += amount;
        if(position > size) size = position;
        return amount == count;
    }

    bool sync() { return !(syncs++ == 0 && first_sync_fails); }
    bool truncate() { size = position; return true; }
};
#endif

#ifdef __DEVKITARM__
bool same_history(const PageHistory& left, const PageHistory& right)
{
    if(left.count != right.count) return false;
    for(int index = 0; index < left.count; ++index) {
        uint32_t left_offset = left.offsets[(left.head + index) % PAGE_HISTORY_MAX];
        uint32_t right_offset = right.offsets[(right.head + index) % PAGE_HISTORY_MAX];
        if(left_offset != right_offset) return false;
    }
    return true;
}
#endif

}

void make_epub_cache_trailer(uint32_t cache_start, uint32_t text_size,
                             uint32_t book_size, uint32_t text_crc32,
                             unsigned char output[EPUB_CACHE_TRAILER_SIZE])
{
    std::memset(output, 0, EPUB_CACHE_TRAILER_SIZE);
    std::memcpy(output, EPUB_CACHE_MAGIC, sizeof(EPUB_CACHE_MAGIC));
    write_u32(output + CACHE_SIZE_AT, EPUB_CACHE_TRAILER_SIZE);
    write_u32(output + CACHE_START_AT, cache_start);
    write_u32(output + CACHE_TEXT_AT, text_size);
    write_u32(output + CACHE_BOOK_AT, book_size);
    write_u32(output + CACHE_TEXT_CRC_AT, text_crc32);
    write_u32(output + CACHE_CHECKSUM_AT, cache_metadata_hash(output));
}

bool parse_epub_cache_trailer(const unsigned char* input, int size,
                              uint32_t payload_size, EpubCacheInfo& info)
{
    if(!input || size != EPUB_CACHE_TRAILER_SIZE ||
       std::memcmp(input, EPUB_CACHE_MAGIC, sizeof(EPUB_CACHE_MAGIC)) != 0 ||
       read_u32(input + CACHE_SIZE_AT) != EPUB_CACHE_TRAILER_SIZE ||
       read_u32(input + CACHE_CHECKSUM_AT) != cache_metadata_hash(input)) return false;
    const uint32_t cache_start = read_u32(input + CACHE_START_AT);
    const uint32_t text_size = read_u32(input + CACHE_TEXT_AT);
    const uint32_t book_size = read_u32(input + CACHE_BOOK_AT);
    if(!text_size || book_size != payload_size || cache_start > book_size ||
       text_size > book_size - cache_start) return false;
    info = {cache_start, text_size, book_size, read_u32(input + CACHE_TEXT_CRC_AT)};
    return true;
}

bool validate_epub_cache_payload(const ByteSource& source, uint32_t cache_start,
                                 uint32_t text_size, uint32_t expected_crc32)
{
    if(!text_size || cache_start > source.size() || text_size > source.size() - cache_start)
        return false;
    uint32_t crc = 0xFFFFFFFFu;
    for(uint32_t offset = 0; offset < text_size; ++offset) {
        unsigned char value = 0;
        if(!source.byte_at(cache_start + offset, value)) return false;
        crc = cache_crc32_byte(crc, value);
    }
    return ~crc == expected_crc32;
}

bool inspect_book_tail(const char* name, uint32_t physical_size,
                       const unsigned char* tail, uint32_t tail_size,
                       BookStorageLayout& layout)
{
    layout = {physical_size, physical_size, 0, 0, 0, 0, false, false};
    if(!supported_book_name(name)) return false;
    uint32_t payload_end = physical_size;
    if(!book_size_without_footer(name, physical_size, tail, tail_size,
                                 payload_end, layout.has_valid_footer,
                                 layout.footer_size)) return true;
    layout.book_size = payload_end;
    layout.footer_offset = payload_end;

    int name_length = 0;
    while(name && name[name_length] && name_length < LIBRARY_NAME_MAX) ++name_length;
    const bool epub = name_length > 5 && name_length < LIBRARY_NAME_MAX &&
                      extension_equal(name + name_length - 5, ".epub");
    if(!epub || !layout.footer_size ||
       payload_end < EPUB_CACHE_TRAILER_SIZE ||
       tail_size < layout.footer_size + EPUB_CACHE_TRAILER_SIZE) return true;

    const unsigned char* trailer = tail + tail_size - layout.footer_size -
                                    EPUB_CACHE_TRAILER_SIZE;
    const uint32_t cache_payload_size = payload_end - EPUB_CACHE_TRAILER_SIZE;
    if(std::memcmp(trailer, EPUB_CACHE_MAGIC, sizeof(EPUB_CACHE_MAGIC)) != 0 ||
       read_u32(trailer + CACHE_SIZE_AT) != EPUB_CACHE_TRAILER_SIZE) return true;
    const uint32_t cache_start = read_u32(trailer + CACHE_START_AT);
    const uint32_t text_size = read_u32(trailer + CACHE_TEXT_AT);
    const uint32_t book_size = read_u32(trailer + CACHE_BOOK_AT);
    if(!text_size || book_size != cache_payload_size || cache_start > book_size ||
       text_size > book_size - cache_start) return true;

    layout.book_size = book_size;
    layout.cache_start = cache_start;
    layout.cache_size = text_size;
    EpubCacheInfo info{};
    layout.has_valid_cache = parse_epub_cache_trailer(
            trailer, EPUB_CACHE_TRAILER_SIZE, cache_payload_size, info);
    if(layout.has_valid_cache) layout.cache_crc32 = info.text_crc32;
    return true;
}

bool txt_book_name(const char* name)
{
    int length = 0;
    while(name && name[length] && length < LIBRARY_NAME_MAX) ++length;
    return length < LIBRARY_NAME_MAX && length > 4 && extension_equal(name + length - 4, ".txt");
}

bool supported_book_name(const char* name)
{
    int length = 0;
    while(name && name[length] && length < LIBRARY_NAME_MAX) ++length;
    if(length >= LIBRARY_NAME_MAX) return false;
    return txt_book_name(name) || (length > 5 && extension_equal(name + length - 5, ".epub"));
}

bool book_size_without_footer(const char* name, uint32_t physical_size,
                              const unsigned char* tail, uint32_t tail_size,
                              uint32_t& logical_size, bool& has_valid_footer,
                              uint32_t& footer_size)
{
    logical_size = physical_size;
    has_valid_footer = false;
    footer_size = 0;
    if(!supported_book_name(name) || !tail) return false;

    constexpr int candidate_sizes[] = {TXT_SAVE_FOOTER_SIZE, TXT_SAVE_FOOTER_V1_SIZE};
    for(int candidate_size : candidate_sizes) {
        if(physical_size < uint32_t(candidate_size) || tail_size < uint32_t(candidate_size)) continue;
        const unsigned char* candidate = tail + tail_size - candidate_size;
        if(!looks_like_txt_save_footer(candidate, candidate_size)) continue;
        TxtSaveFooter footer{};
        has_valid_footer = parse_txt_save_footer(candidate, candidate_size, footer);
        footer_size = uint32_t(candidate_size);
        logical_size = physical_size - footer_size;
        return true;
    }
    return false;
}

const char* save_result_string(bool saved)
{
    return saved ? "Saved" : "Save failed";
}

#ifndef __DEVKITARM__
EpubCacheWriteTestResult epub_cache_write_transaction_for_tests(
        uint32_t text_size, uint32_t existing_footer_size,
        int failed_write, bool first_sync_fails)
{
    unsigned char previous[TXT_SAVE_FOOTER_SIZE];
    unsigned char replacement[TXT_SAVE_FOOTER_SIZE];
    unsigned char text[64];
    unsigned char archive_bytes[64];
    unsigned char scratch[64];
    if(text_size > sizeof(text)) return {0, false, false};
    std::memset(previous, 0xA5, sizeof(previous));
    std::memset(replacement, 0x5A, sizeof(replacement));
    std::memset(archive_bytes, 0x3C, sizeof(archive_bytes));
    for(uint32_t index = 0; index < text_size; ++index)
        text[index] = static_cast<unsigned char>('a' + index % 26);

    MemorySource text_source(text, text_size);
    MemorySource archive_source(archive_bytes, sizeof(archive_bytes));
    MemoryFooterOps ops{};
    ops.failed_write = failed_write;
    ops.first_sync_fails = first_sync_fails;
    if(existing_footer_size) {
        std::memcpy(ops.data + 8, previous, existing_footer_size);
        ops.size = 8 + existing_footer_size;
    }
    const bool success = write_epub_cache_transaction(
            ops, 8, archive_source, 20, 20, 1, text_source,
            replacement, previous, existing_footer_size, scratch, sizeof(scratch));
    const bool restored = existing_footer_size &&
                          ops.size == 8 + existing_footer_size &&
                          std::memcmp(ops.data + 8, previous, existing_footer_size) == 0;
    return {ops.size, success, restored};
}

FooterWriteTestResult footer_write_transaction_for_tests(uint32_t existing_footer_size,
                                                         int first_write_limit,
                                                         bool first_sync_fails)
{
    unsigned char previous[TXT_SAVE_FOOTER_SIZE];
    unsigned char replacement[TXT_SAVE_FOOTER_SIZE];
    std::memset(previous, 0xA5, sizeof(previous));
    std::memset(replacement, 0x5A, sizeof(replacement));

    MemoryFooterOps ops{};
    ops.first_write_limit = first_write_limit;
    ops.first_sync_fails = first_sync_fails;
    if(existing_footer_size) {
        std::memcpy(ops.data + 8, previous, existing_footer_size);
        ops.size = 8 + existing_footer_size;
    }

    bool success = write_footer_transaction(
            ops, 8, replacement, previous, existing_footer_size);
    bool restored = existing_footer_size && ops.size == 8 + existing_footer_size &&
                    std::memcmp(ops.data + 8, previous, existing_footer_size) == 0;
    return {ops.size, success, restored};
}
#endif

bool storage_init()
{
    name_count = 0;
#ifdef __DEVKITARM__
    REG_WAITCNT = 0x40c0;
    set_supercard_mode(MAPPED_SDRAM, true, true);
    t_card_info info;
    if(sdcard_init(&info) != 0 || f_mount(&fatfs, "0:", 1) != FR_OK) return false;
    DIR directory;
    FILINFO entry;
    if(f_opendir(&directory, "/") != FR_OK) return false;
    while(name_count < LIBRARY_MAX_FILES && f_readdir(&directory, &entry) == FR_OK && entry.fname[0]) {
        if(!(entry.fattrib & AM_DIR) && supported_book_name(entry.fname)) {
            int index = 0;
            while(entry.fname[index] && index < LIBRARY_NAME_MAX - 1) {
                names[name_count][index] = entry.fname[index];
                ++index;
            }
            names[name_count][index] = 0;
            ++name_count;
        }
    }
    f_closedir(&directory);
    return true;
#else
    return false;
#endif
}

int library_count() { return name_count; }
const char* library_name(int index)
{
    return index >= 0 && index < name_count ? names[index] : nullptr;
}

ReaderFile::ReaderFile() :
    _cache_start(0), _cache_size(0), _size(0), _physical_size(0), _footer_size(0),
    _footer_offset(0), _epub_cache_start(0), _epub_cache_size(0),
    _has_footer(false), _has_valid_cache(false), _open(false), _name{}
{
}

ReaderFile::~ReaderFile() { close(); }

bool ReaderFile::open_read_only(const char* filename)
{
    close();
#ifdef __DEVKITARM__
    if(!supported_book_name(filename) ||
       f_open(&_file, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;
    _physical_size = uint32_t(f_size(&_file));
    _size = _physical_size;
    _footer_offset = _physical_size;
    _footer_size = 0;
    _epub_cache_start = 0;
    _epub_cache_size = 0;
    _has_footer = false;
    _has_valid_cache = false;
    _open = true;
    int index = 0;
    while(filename[index] && index < LIBRARY_NAME_MAX - 1) {
        _name[index] = filename[index];
        ++index;
    }
    _name[index] = 0;

    uint32_t tail_size = _physical_size < uint32_t(TXT_SAVE_FOOTER_SIZE + EPUB_CACHE_TRAILER_SIZE) ?
                         _physical_size : uint32_t(TXT_SAVE_FOOTER_SIZE + EPUB_CACHE_TRAILER_SIZE);
    if(tail_size >= TXT_SAVE_FOOTER_V1_SIZE) {
        unsigned char tail[TXT_SAVE_FOOTER_SIZE + EPUB_CACHE_TRAILER_SIZE];
        UINT read = 0;
        if(f_lseek(&_file, _physical_size - tail_size) != FR_OK ||
           f_read(&_file, tail, tail_size, &read) != FR_OK || read != tail_size) {
            close();
            return false;
        }
        BookStorageLayout layout{};
        if(!inspect_book_tail(_name, _physical_size, tail, tail_size, layout)) {
            close();
            return false;
        }
        _size = layout.book_size;
        _footer_offset = layout.footer_offset;
        _footer_size = layout.footer_size;
        _has_footer = layout.has_valid_footer;
        _epub_cache_start = layout.cache_start;
        _epub_cache_size = layout.cache_size;
        _cache_size = 0;
        _has_valid_cache = layout.has_valid_cache && validate_epub_cache_payload(
                *this, layout.cache_start, layout.cache_size, layout.cache_crc32);
        _cache_size = 0;
    }
    return true;
#else
    (void) filename;
    return false;
#endif
}

void ReaderFile::close()
{
#ifdef __DEVKITARM__
    if(_open) f_close(&_file);
#endif
    _open = false;
    _size = 0;
    _physical_size = 0;
    _footer_size = 0;
    _footer_offset = 0;
    _epub_cache_start = 0;
    _epub_cache_size = 0;
    _cache_size = 0;
    _has_footer = false;
    _has_valid_cache = false;
    _name[0] = 0;
}

bool ReaderFile::physical_byte_at(uint32_t offset, unsigned char& value) const
{
    if(!_open || offset >= _physical_size) return false;
    if(offset < _cache_start || offset >= _cache_start + uint32_t(_cache_size)) {
#ifdef __DEVKITARM__
        _cache_start = offset & ~uint32_t(511);
        _cache_size = 0;
        if(f_lseek(&_file, _cache_start) != FR_OK) return false;
        UINT read = 0;
        if(f_read(&_file, _cache, sizeof(_cache), &read) != FR_OK) return false;
        _cache_size = int(read);
#else
        return false;
#endif
    }
    if(offset - _cache_start >= uint32_t(_cache_size)) return false;
    value = _cache[offset - _cache_start];
    return true;
}

bool ReaderFile::byte_at(uint32_t offset, unsigned char& value) const
{
    return offset < _size && physical_byte_at(offset, value);
}

bool ReaderFile::optimized_byte_at(uint32_t offset, unsigned char& value) const
{
    return _has_valid_cache && offset < _epub_cache_size &&
           physical_byte_at(_epub_cache_start + offset, value);
}

bool ReaderFile::saved_footer(TxtSaveFooter& footer) const
{
    if(!_open || !_has_footer || !_footer_size) return false;
#ifdef __DEVKITARM__
    unsigned char tail[TXT_SAVE_FOOTER_SIZE];
    UINT read = 0;
    if(f_lseek(&_file, _footer_offset) != FR_OK ||
       f_read(&_file, tail, _footer_size, &read) != FR_OK || read != _footer_size) return false;
    _cache_size = 0;
    return parse_txt_save_footer(tail, int(_footer_size), footer);
#else
    (void) footer;
    return false;
#endif
}

bool ReaderFile::save_footer(const TxtSaveFooter& footer, const ByteSource* optimized_source)
{
    if(!_open || !supported_book_name(_name)) return false;
#ifdef __DEVKITARM__
    char filename[LIBRARY_NAME_MAX]{};
    std::memcpy(filename, _name, sizeof(filename));
    unsigned char replacement[TXT_SAVE_FOOTER_SIZE];
    make_txt_save_footer(footer, replacement);
    const uint32_t previous_size = _footer_size;
    if(previous_size) {
        UINT read = 0;
        if(f_lseek(&_file, _footer_offset) != FR_OK ||
           f_read(&_file, _previous_footer, previous_size, &read) != FR_OK ||
           read != previous_size) {
            _cache_size = 0;
            return false;
        }
    }

    const bool cache_present = _epub_cache_size != 0;
    uint32_t central_offset = 0;
    uint32_t central_size = 0;
    uint16_t entry_count = 0;
    const bool create_cache = optimized_source && !txt_book_name(_name) &&
                              !cache_present && optimized_source->size() &&
                              optimized_source->cache_archive_layout(
                                      central_offset, central_size, entry_count);
    _cache_size = 0;
    if(f_close(&_file) != FR_OK) {
        _open = false;
        open_read_only(filename);
        return false;
    }
    _open = false;
    if(f_open(&_file, filename, FA_READ | FA_WRITE | FA_OPEN_EXISTING) != FR_OK) {
        open_read_only(filename);
        return false;
    }
    _open = true;
    FatFooterOps ops{_file};
    const bool written = create_cache ?
            write_epub_cache_transaction(
                    ops, _size, *this, central_offset, central_size, entry_count,
                    *optimized_source, replacement, _previous_footer, previous_size,
                    _write_cache, sizeof(_write_cache)) :
            write_footer_transaction(
                    ops, _footer_offset, replacement, _previous_footer, previous_size);
    const bool closed = f_close(&_file) == FR_OK;
    _open = false;
    const bool reopened = open_read_only(filename);
    if(!written || !closed || !reopened) return false;
    if(create_cache && optimized_size() != optimized_source->size()) return false;

    TxtSaveFooter verified{};
    return saved_footer(verified) && verified.byte_offset == footer.byte_offset &&
           verified.settings.line_spacing == footer.settings.line_spacing &&
           verified.settings.top_margin == footer.settings.top_margin &&
           verified.settings.bottom_margin == footer.settings.bottom_margin &&
           same_history(verified.history, footer.history);
#else
    (void) footer;
    (void) optimized_source;
    return false;
#endif
}

}
