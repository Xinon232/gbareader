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
        char a = *value++, b = *extension++;
        if(a >= 'A' && a <= 'Z') a = char(a + ('a' - 'A'));
        if(a != b) return false;
    }
    return ! *value && ! *extension;
}
}

bool supported_book_name(const char* name)
{
    int length = 0;
    while(name && name[length] && length < LIBRARY_NAME_MAX) ++length;
    if(length >= LIBRARY_NAME_MAX) return false;
    return (length > 4 && extension_equal(name + length - 4, ".txt")) ||
           (length > 5 && extension_equal(name + length - 5, ".epub"));
}

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
        if(! (entry.fattrib & AM_DIR) && supported_book_name(entry.fname)) {
            int i = 0;
            while(entry.fname[i] && i < LIBRARY_NAME_MAX - 1) {
                names[name_count][i] = entry.fname[i];
                ++i;
            }
            names[name_count][i] = 0;
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
const char* library_name(int index) { return index >= 0 && index < name_count ? names[index] : nullptr; }

ReaderFile::ReaderFile() : _cache_start(0), _cache_size(0), _size(0), _open(false) {}
ReaderFile::~ReaderFile() { close(); }

bool ReaderFile::open_read_only(const char* filename)
{
    close();
#ifdef __DEVKITARM__
    if(! supported_book_name(filename) || f_open(&_file, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) return false;
    _size = uint32_t(f_size(&_file));
    _open = true;
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
    _cache_size = 0;
}

bool ReaderFile::byte_at(uint32_t offset, unsigned char& value) const
{
    if(! _open || offset >= _size) return false;
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

}
