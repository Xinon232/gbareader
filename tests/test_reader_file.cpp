#include "reader_file.h"
#include <cassert>
#include <cstdio>

int main()
{
    assert(reader::supported_book_name("book.txt"));
    assert(reader::supported_book_name("BOOK.TXT"));
    assert(reader::supported_book_name("novel.epub"));
    assert(reader::supported_book_name("NOVEL.EPUB"));
    assert(! reader::supported_book_name("fake.epub.zip"));
    assert(! reader::supported_book_name(".epub"));
    std::puts("PASS: library extensions");
}
