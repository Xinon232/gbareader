#include "reader_browse.h"
#include "reader_file.h"

#ifdef __DEVKITARM__
#include "ff.h"
#define BROWSE_EWRAM __attribute__((section(".sbss")))
#else
#define BROWSE_EWRAM
#endif

namespace reader {
namespace {

int length(const char* s) { int n = 0; while(s[n]) ++n; return n; }

bool same_ignoring_case(const char* a, const char* b)
{
    for(; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if(x >= 'A' && x <= 'Z') x = char(x + 'a' - 'A');
        if(y >= 'A' && y <= 'Z') y = char(y + 'a' - 'A');
        if(x != y) return false;
    }
    return !*a && !*b;
}

BROWSE_EWRAM char folder_path[BROWSE_PATH_MAX];
BROWSE_EWRAM char names[BROWSE_WINDOW][BROWSE_NAME_MAX];
int entry_count, folder_count, window_start, window_count;

#ifdef __DEVKITARM__
BROWSE_EWRAM unsigned char copy_buffer[16 * 1024];

void copy_name(char (&out)[BROWSE_NAME_MAX], const char* name)
{
    int i = 0;
    for(; name[i] && i < BROWSE_NAME_MAX - 1; ++i) out[i] = name[i];
    out[i] = 0;
}

// One pass over the folder: counts folders and books, and keeps the names of
// entries first .. first + BROWSE_WINDOW - 1 (folders come before books).
bool read_folder(int first, int known_folders, int& folders, int& books, int& kept)
{
    folders = books = kept = 0;
    DIR directory;
    FILINFO entry;
    if(f_opendir(&directory, folder_path) != FR_OK) return false;
    bool ok = true;
    for(;;) {
        if(f_readdir(&directory, &entry) != FR_OK) { ok = false; break; }
        if(!entry.fname[0]) break;
        const bool is_folder = entry.fattrib & AM_DIR;
        if(!browse_listed(folder_path, entry.fname, is_folder, entry.fattrib & (AM_HID | AM_SYS)))
            continue;
        const int index = is_folder ? folders++ : known_folders + books++;
        if(index >= first && index - first < BROWSE_WINDOW && known_folders >= 0) {
            copy_name(names[index - first], entry.fname);
            if(index - first + 1 > kept) kept = index - first + 1;
        }
    }
    if(f_closedir(&directory) != FR_OK) ok = false;
    return ok;
}
#endif

}

bool browse_join(char (&out)[BROWSE_PATH_MAX], const char* folder, const char* name)
{
    const int a = length(folder), b = length(name);
    const bool slash = a == 0 || folder[a - 1] != '/';
    if(!b || a + int(slash) + b >= BROWSE_PATH_MAX) return false;
    int at = 0;
    for(int i = 0; i < a; ++i) out[at++] = folder[i];
    if(slash) out[at++] = '/';
    for(int i = 0; i < b; ++i) out[at++] = name[i];
    out[at] = 0;
    return true;
}

bool browse_parent(char (&path)[BROWSE_PATH_MAX])
{
    int n = length(path);
    while(n > 1 && path[n - 1] == '/') --n;
    if(n <= 1) { path[0] = '/'; path[1] = 0; return false; }
    while(n > 0 && path[n - 1] != '/') --n;
    if(n > 1) --n;
    path[n > 0 ? n : 1] = 0;
    if(n == 0) path[0] = '/';
    return true;
}

const char* browse_leaf(const char* path)
{
    const char* leaf = path;
    for(const char* p = path; *p; ++p) if(*p == '/' && p[1]) leaf = p + 1;
    return *leaf == '/' ? leaf + 1 : leaf;
}

bool browse_listed(const char* folder, const char* name, bool is_folder, bool hidden)
{
    if(hidden || !name[0] || name[0] == '.') return false;
    if(!is_folder) return supported_book_name(name);
    const bool root = folder[0] == '/' && !folder[1];
    return !(root && same_ignoring_case(name, "gbareader"));
}

bool browse_open(const char* folder)
{
    entry_count = folder_count = window_start = window_count = 0;
    int n = length(folder);
    if(n >= BROWSE_PATH_MAX) return false;
    for(int i = 0; i <= n; ++i) folder_path[i] = folder[i];
#ifdef __DEVKITARM__
    int folders = 0, books = 0, kept = 0;
    // First pass counts folders; the second keeps names in folder-first order.
    if(!read_folder(0, -1, folders, books, kept)) return false;
    if(!read_folder(0, folders, folders, books, kept)) return false;
    folder_count = folders;
    entry_count = folders + books;
    window_count = kept;
    return true;
#else
    return false;
#endif
}

const char* browse_folder() { return folder_path; }
int browse_count() { return entry_count; }
bool browse_is_folder(int index) { return index >= 0 && index < folder_count; }

const char* browse_name(int index)
{
    if(index < 0 || index >= entry_count) return nullptr;
    if(index < window_start || index >= window_start + window_count) {
#ifdef __DEVKITARM__
        int first = index - BROWSE_WINDOW / 2;
        if(first < 0) first = 0;
        int folders = 0, books = 0, kept = 0;
        if(!read_folder(first, folder_count, folders, books, kept) ||
           folders != folder_count || folders + books != entry_count || index - first >= kept) {
            window_count = 0;
            return nullptr;
        }
        window_start = first;
        window_count = kept;
#else
        return nullptr;
#endif
    }
    return names[index - window_start];
}

ImportResult import_book(const char* source, const char* name,
                         void (*progress)(void* context, int percent), void* context)
{
#ifdef __DEVKITARM__
    char target[BROWSE_PATH_MAX];
    if(!browse_join(target, "/gbareader", name)) return ImportResult::WRITE_FAILED;
    FILINFO info;
    if(f_stat(target, &info) == FR_OK) return ImportResult::EXISTS;
    const FRESULT made = f_mkdir("/gbareader");
    if(made != FR_OK && made != FR_EXIST) return ImportResult::WRITE_FAILED;
    FIL in, out;
    if(f_open(&in, source, FA_READ) != FR_OK) return ImportResult::READ_FAILED;
    if(f_open(&out, target, FA_WRITE | FA_CREATE_NEW) != FR_OK) {
        f_close(&in);
        return ImportResult::WRITE_FAILED;
    }
    const FSIZE_t total = f_size(&in);
    FSIZE_t done = 0;
    ImportResult result = ImportResult::COPIED;
    int shown = -1;
    while(done < total) {
        const int percent = total ? int((uint64_t(done) * 100) / total) : 100;
        if(progress && percent != shown) { progress(context, percent); shown = percent; }
        UINT want = total - done > sizeof(copy_buffer) ? sizeof(copy_buffer) : UINT(total - done);
        UINT got = 0, put = 0;
        if(f_read(&in, copy_buffer, want, &got) != FR_OK || got != want) {
            result = ImportResult::READ_FAILED;
            break;
        }
        if(f_write(&out, copy_buffer, got, &put) != FR_OK || put != got) {
            result = ImportResult::WRITE_FAILED;
            break;
        }
        done += got;
    }
    f_close(&in);
    if(f_close(&out) != FR_OK && result == ImportResult::COPIED) result = ImportResult::WRITE_FAILED;
    if(result != ImportResult::COPIED) f_unlink(target);
    else if(progress) progress(context, 100);
    return result;
#else
    (void)source; (void)name; (void)progress; (void)context;
    return ImportResult::READ_FAILED;
#endif
}

const char* import_result_string(ImportResult result)
{
    switch(result) {
    case ImportResult::COPIED: return "Imported";
    case ImportResult::EXISTS: return "Already in /gbareader";
    case ImportResult::READ_FAILED: return "Import failed: cannot read book";
    default: return "Import failed: cannot write to card";
    }
}

}
