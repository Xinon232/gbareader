#include "reader_browse.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using namespace reader;
int main()
{
    char path[BROWSE_PATH_MAX];
    assert(browse_join(path, "/", "Downloads") && !std::strcmp(path, "/Downloads"));
    assert(browse_join(path, "/Downloads", u8"Novels é") && !std::strcmp(path, u8"/Downloads/Novels é"));
    assert(!browse_join(path, "/", ""));
    const std::string deep(BROWSE_PATH_MAX - 3, 'a');
    assert(browse_join(path, "/", deep.c_str()) && std::strlen(path) == BROWSE_PATH_MAX - 2);
    assert(!browse_join(path, "/a", deep.c_str())); // Too long: refused, never cut.
    std::strcpy(path, "/a/b/c");
    assert(browse_parent(path) && !std::strcmp(path, "/a/b"));
    assert(browse_parent(path) && !std::strcmp(path, "/a"));
    assert(browse_parent(path) && !std::strcmp(path, "/"));
    assert(!browse_parent(path) && !std::strcmp(path, "/"));
    assert(!std::strcmp(browse_leaf("/Downloads/Moby Dick.epub"), "Moby Dick.epub"));
    assert(!std::strcmp(browse_leaf("/book.txt"), "book.txt") && !std::strcmp(browse_leaf("/"), ""));
    // Only TXT / EPUB books and visible folders; /gbareader itself is not offered.
    assert(browse_listed("/", "Downloads", true, false));
    assert(!browse_listed("/", "gbareader", true, false) && !browse_listed("/", "GBAREADER", true, false));
    assert(browse_listed("/Downloads", "gbareader", true, false));
    assert(!browse_listed("/", "System Volume Information", true, true));
    assert(!browse_listed("/", ".Trashes", true, false) && !browse_listed("/", "._book.txt", false, false));
    assert(browse_listed("/", "Book.TXT", false, false) && browse_listed("/x", "b.epub", false, false));
    assert(!browse_listed("/", "song.mp3", false, false) && !browse_listed("/", "b.txt.sav", false, false));
    assert(!std::strcmp(import_result_string(ImportResult::EXISTS), "Already in /gbareader"));
    std::puts("PASS: import browser paths, parent/leaf, listed entries, result messages");
}
