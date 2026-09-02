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

void test_epub_cache_trailer_round_trip_and_corruption()
{
    unsigned char trailer[reader::EPUB_CACHE_TRAILER_SIZE];
    reader::make_epub_cache_trailer(12345, 6789, 20000, 0xA1B2C3D4u, trailer);

    reader::EpubCacheInfo info{};
    assert(reader::parse_epub_cache_trailer(trailer, sizeof(trailer), 20000,
                                            info));
    assert(info.cache_start == 12345);
    assert(info.text_size == 6789);
    assert(info.book_size == 20000);
    assert(info.text_crc32 == 0xA1B2C3D4u);

    trailer[reader::EPUB_CACHE_TRAILER_SIZE - 1] ^= 1;
    assert(!reader::parse_epub_cache_trailer(trailer, sizeof(trailer), 20000,
                                             info));
}

void test_first_epub_cache_write_is_transactional()
{
    const uint32_t expected = 8 + 37 + 20 + 22 + reader::EPUB_CACHE_TRAILER_SIZE +
                              reader::TXT_SAVE_FOOTER_SIZE;
    const reader::EpubCacheWriteTestResult success =
            reader::epub_cache_write_transaction_for_tests(37, 0, -1, false);
    assert(success.success);
    assert(success.physical_size == expected);

    for(int failed_write = 0; failed_write < 5; ++failed_write) {
        const reader::EpubCacheWriteTestResult failed =
                reader::epub_cache_write_transaction_for_tests(
                        37, reader::TXT_SAVE_FOOTER_SIZE, failed_write, false);
        assert(!failed.success);
        assert(failed.physical_size == 8 + reader::TXT_SAVE_FOOTER_SIZE);
        assert(failed.old_footer_restored);
    }

    const reader::EpubCacheWriteTestResult failed_sync =
            reader::epub_cache_write_transaction_for_tests(
                    37, reader::TXT_SAVE_FOOTER_V1_SIZE, -1, true);
    assert(!failed_sync.success);
    assert(failed_sync.physical_size == 8 + reader::TXT_SAVE_FOOTER_V1_SIZE);
    assert(failed_sync.old_footer_restored);
}

void test_epub_cache_payload_crc_rejects_corruption_and_bounds()
{
    unsigned char payload[] = "123456789";
    reader::MemorySource source(payload, 9);
    assert(reader::validate_epub_cache_payload(source, 0, 9, 0xCBF43926u));
    payload[4] ^= 1;
    assert(!reader::validate_epub_cache_payload(source, 0, 9, 0xCBF43926u));
    assert(!reader::validate_epub_cache_payload(source, 4, 6, 0));
}

void test_epub_cache_layout_hides_cache_and_falls_back_safely()
{
    const unsigned char untouched_tail[] = "ordinary ZIP tail";
    reader::BookStorageLayout untouched{};
    assert(reader::inspect_book_tail("book.epub", 1000, untouched_tail,
                                     sizeof(untouched_tail) - 1, untouched));
    assert(untouched.book_size == 1000);
    assert(untouched.footer_offset == 1000);
    assert(!untouched.has_valid_footer);
    assert(!untouched.has_valid_cache);

    reader::TxtSaveFooter saved{};
    saved.settings = reader::default_settings();
    unsigned char tail[reader::EPUB_CACHE_TRAILER_SIZE + reader::TXT_SAVE_FOOTER_SIZE];
    reader::make_epub_cache_trailer(1000, 200, 1300, 0x12345678u, tail);
    reader::make_txt_save_footer(saved, tail + reader::EPUB_CACHE_TRAILER_SIZE);

    reader::BookStorageLayout layout{};
    const uint32_t physical_size = 1300 + sizeof(tail);
    assert(reader::inspect_book_tail("book.epub", physical_size, tail, sizeof(tail),
                                     layout));
    assert(layout.book_size == 1300);
    assert(layout.footer_offset == 1300 + reader::EPUB_CACHE_TRAILER_SIZE);
    assert(layout.footer_size == reader::TXT_SAVE_FOOTER_SIZE);
    assert(layout.cache_start == 1000);
    assert(layout.cache_size == 200);
    assert(layout.cache_crc32 == 0x12345678u);
    assert(layout.has_valid_footer);
    assert(layout.has_valid_cache);

    tail[reader::EPUB_CACHE_TRAILER_SIZE - 1] ^= 1;
    assert(reader::inspect_book_tail("book.epub", physical_size, tail, sizeof(tail),
                                     layout));
    assert(layout.book_size == 1300);
    assert(layout.has_valid_footer);
    assert(!layout.has_valid_cache);
    assert(layout.cache_crc32 == 0);
}

}

int main()
{
    test_library_extensions();
    test_v2_and_v1_footer_sizes_are_hidden();
    test_failed_replacement_restores_exact_previous_footer_size();
    test_epub_cache_trailer_round_trip_and_corruption();
    test_first_epub_cache_write_is_transactional();
    test_epub_cache_payload_crc_rejects_corruption_and_bounds();
    test_epub_cache_layout_hides_cache_and_falls_back_safely();
    assert(std::strcmp(reader::save_result_string(true), "Saved") == 0);
    assert(std::strcmp(reader::save_result_string(false), "Save failed") == 0);
    std::puts("PASS: library and versioned footer I/O");
}
