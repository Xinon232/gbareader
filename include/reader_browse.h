#pragma once
// Import browser: walks the SD card from its root and copies a TXT or EPUB
// book into /gbareader. Folders are listed first, then books, each in card
// order. Hidden and system entries and /gbareader itself are left out.
#include <cstdint>

namespace reader {

constexpr int BROWSE_PATH_MAX = 512;
// Names held in RAM at once; others are reread from the folder when shown.
constexpr int BROWSE_WINDOW = 24;
constexpr int BROWSE_NAME_MAX = 256;

// Path helpers (host-tested). Paths are absolute, "/" is the root.
bool browse_join(char (&out)[BROWSE_PATH_MAX], const char* folder, const char* name);
// "/a/b" -> "/a", "/a" -> "/". False at the root.
bool browse_parent(char (&path)[BROWSE_PATH_MAX]);
// The last element of the path ("" for the root).
const char* browse_leaf(const char* path);
// Whether an entry is listed (folder: not hidden, not /gbareader at the
// root; file: a supported book name).
bool browse_listed(const char* folder, const char* name, bool is_folder, bool hidden);

// Opens folder for listing. False when it cannot be read.
bool browse_open(const char* folder);
const char* browse_folder();
int browse_count();
bool browse_is_folder(int index);
// Null when the folder cannot be reread.
const char* browse_name(int index);

enum class ImportResult { COPIED, EXISTS, READ_FAILED, WRITE_FAILED };
// Copies source (a book path) to /gbareader/name. progress(percent) is
// called while copying. A failed copy leaves no partial file behind.
ImportResult import_book(const char* source, const char* name,
                         void (*progress)(void* context, int percent), void* context);
const char* import_result_string(ImportResult result);

}
