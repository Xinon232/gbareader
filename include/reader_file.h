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
bool storage_init(); int library_count(); const char* library_name(int index); bool supported_book_name(const char* name); bool txt_book_name(const char* name);
class ReaderFile final : public ByteSource {
public:
 ReaderFile(); ~ReaderFile() override; bool open_read_only(const char* filename); void close(); bool is_open() const{return _open;} uint32_t size() const override{return _size;} bool byte_at(uint32_t offset,unsigned char& value) const override;
 bool saved_footer(TxtSaveFooter& footer) const; bool save_footer(const TxtSaveFooter& footer);
private:
#ifdef __DEVKITARM__
 mutable FIL _file;
#endif
 mutable uint32_t _cache_start; mutable int _cache_size; mutable unsigned char _cache[512]; uint32_t _size; uint32_t _physical_size; bool _has_footer; bool _open; char _name[LIBRARY_NAME_MAX];
};
}
