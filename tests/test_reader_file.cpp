#include "reader_file.h"
#include <cassert>
#include <cstdio>

int main()
{
    assert(reader::supported_book_name("book.txt"));
    assert(reader::supported_book_name("BOOK.TXT"));
    assert(reader::supported_book_name("novel.epub"));
    assert(reader::supported_book_name("NOVEL.EPUB"));
    char long_epub[256];
    for(int index = 0; index < 250; ++index) long_epub[index] = 'a';
    const char suffix[] = ".epub";
    for(int index = 0; index < 6; ++index) long_epub[250 + index] = suffix[index];
    assert(reader::supported_book_name(long_epub));
    assert(! reader::supported_book_name("fake.epub.zip"));
    assert(! reader::supported_book_name(".epub"));
    std::puts("PASS: library extensions");
}
