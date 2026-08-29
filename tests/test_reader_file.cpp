#include "reader_file.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace {

const unsigned char legacy_v1_footer[] =
    "\n[GBAR-SAVE:1;O= 0000123456;S=1;T=1;B=1;C=59AAEAA4                                             \n";
static_assert(sizeof(legacy_v1_footer) == reader::TXT_SAVE_FOOTER_V1_SIZE + 1);

void test_library_extensions()
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
}

void test_v2_and_v1_footer_sizes_are_hidden()
{
    reader::TxtSaveFooter saved{};
    saved.byte_offset = 1234;
    saved.settings = {2, 3, 4};
    saved.history.count = 2;
    saved.history.offsets[0] = 100;
    saved.history.offsets[1] = 500;
    unsigned char v2[reader::TXT_SAVE_FOOTER_SIZE];
    reader::make_txt_save_footer(saved, v2);

    uint32_t logical_size = 0;
    uint32_t footer_size = 0;
    bool has_valid_footer = false;
    assert(reader::book_size_without_footer("novel.epub", 5000, v2, sizeof(v2),
                                            logical_size, has_valid_footer, footer_size));
    assert(logical_size == 5000 - reader::TXT_SAVE_FOOTER_SIZE);
    assert(footer_size == reader::TXT_SAVE_FOOTER_SIZE);
    assert(has_valid_footer);

    assert(reader::book_size_without_footer(
            "book.txt", 5000, legacy_v1_footer, reader::TXT_SAVE_FOOTER_V1_SIZE,
            logical_size, has_valid_footer, footer_size));
    assert(logical_size == 5000 - reader::TXT_SAVE_FOOTER_V1_SIZE);
    assert(footer_size == reader::TXT_SAVE_FOOTER_V1_SIZE);
    assert(has_valid_footer);

    unsigned char full_tail[reader::TXT_SAVE_FOOTER_SIZE];
    std::memset(full_tail, 'x', sizeof(full_tail));
    std::memcpy(full_tail + sizeof(full_tail) - reader::TXT_SAVE_FOOTER_V1_SIZE,
                legacy_v1_footer, reader::TXT_SAVE_FOOTER_V1_SIZE);
    assert(reader::book_size_without_footer("book.txt", 5000, full_tail, sizeof(full_tail),
                                            logical_size, has_valid_footer, footer_size));
    assert(logical_size == 5000 - reader::TXT_SAVE_FOOTER_V1_SIZE);
    assert(footer_size == reader::TXT_SAVE_FOOTER_V1_SIZE);
    assert(has_valid_footer);

    v2[100] ^= 1;
    assert(reader::book_size_without_footer("novel.epub", 5000, v2, sizeof(v2),
                                            logical_size, has_valid_footer, footer_size));
    assert(logical_size == 5000 - reader::TXT_SAVE_FOOTER_SIZE);
    assert(! has_valid_footer);
}

void test_failed_replacement_restores_exact_previous_footer_size()
{
    const reader::FooterWriteTestResult partial_append =
            reader::footer_write_transaction_for_tests(0, 17, false);
    assert(! partial_append.success);
    assert(partial_append.physical_size == 8);

    const reader::FooterWriteTestResult partial_v1_replace =
            reader::footer_write_transaction_for_tests(reader::TXT_SAVE_FOOTER_V1_SIZE,
                                                        17, false);
    assert(! partial_v1_replace.success);
    assert(partial_v1_replace.physical_size == 8 + reader::TXT_SAVE_FOOTER_V1_SIZE);
    assert(partial_v1_replace.old_footer_restored);

    const reader::FooterWriteTestResult failed_v2_sync =
            reader::footer_write_transaction_for_tests(reader::TXT_SAVE_FOOTER_SIZE,
                                                        reader::TXT_SAVE_FOOTER_SIZE,
                                                        true);
    assert(! failed_v2_sync.success);
    assert(failed_v2_sync.physical_size == 8 + reader::TXT_SAVE_FOOTER_SIZE);
    assert(failed_v2_sync.old_footer_restored);

    const reader::FooterWriteTestResult successful_upgrade =
            reader::footer_write_transaction_for_tests(reader::TXT_SAVE_FOOTER_V1_SIZE,
                                                        reader::TXT_SAVE_FOOTER_SIZE,
                                                        false);
    assert(successful_upgrade.success);
    assert(successful_upgrade.physical_size == 8 + reader::TXT_SAVE_FOOTER_SIZE);
}

}

int main()
{
    test_library_extensions();
    test_v2_and_v1_footer_sizes_are_hidden();
    test_failed_replacement_restores_exact_previous_footer_size();
    assert(std::strcmp(reader::save_result_string(true), "Saved") == 0);
    assert(std::strcmp(reader::save_result_string(false), "Save failed") == 0);
    std::puts("PASS: library and versioned footer I/O");
}
