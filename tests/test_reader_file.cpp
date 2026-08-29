#include "reader_file.h"
#include <cassert>
#include <cstdio>
#include <cstring>

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

    reader::TxtSaveFooter saved{1234, {2, 3, 4}};
    unsigned char footer[reader::TXT_SAVE_FOOTER_SIZE];
    reader::make_txt_save_footer(saved, footer);
    uint32_t logical_size = 0;
    bool has_valid_footer = false;
    assert(reader::book_size_without_footer(
            "novel.epub", 5000, footer, logical_size, has_valid_footer));
    assert(logical_size == 5000 - reader::TXT_SAVE_FOOTER_SIZE);
    assert(has_valid_footer);

    footer[42] ^= 1;
    assert(reader::book_size_without_footer(
            "novel.epub", 5000, footer, logical_size, has_valid_footer));
    assert(logical_size == 5000 - reader::TXT_SAVE_FOOTER_SIZE);
    assert(! has_valid_footer);
    assert(std::strcmp(reader::save_result_string(true), "Saved") == 0);
    assert(std::strcmp(reader::save_result_string(false), "Save failed") == 0);

    const reader::FooterWriteTestResult partial_append =
            reader::footer_write_transaction_for_tests(false, 17, false);
    assert(!partial_append.success);
    assert(partial_append.physical_size == 8);

    const reader::FooterWriteTestResult partial_replace =
            reader::footer_write_transaction_for_tests(true, 17, false);
    assert(!partial_replace.success);
    assert(partial_replace.physical_size == 8 + reader::TXT_SAVE_FOOTER_SIZE);
    assert(partial_replace.old_footer_restored);

    const reader::FooterWriteTestResult failed_sync =
            reader::footer_write_transaction_for_tests(true,
                                                        reader::TXT_SAVE_FOOTER_SIZE,
                                                        true);
    assert(!failed_sync.success);
    assert(failed_sync.old_footer_restored);

    const reader::FooterWriteTestResult successful_append =
            reader::footer_write_transaction_for_tests(false,
                                                        reader::TXT_SAVE_FOOTER_SIZE,
                                                        false);
    assert(successful_append.success);
    assert(successful_append.physical_size == 8 + reader::TXT_SAVE_FOOTER_SIZE);
    const reader::FooterWriteTestResult successful_replace =
            reader::footer_write_transaction_for_tests(true,
                                                        reader::TXT_SAVE_FOOTER_SIZE,
                                                        false);
    assert(successful_replace.success);
    assert(successful_replace.physical_size == 8 + reader::TXT_SAVE_FOOTER_SIZE);
    std::puts("PASS: library extensions");
}
