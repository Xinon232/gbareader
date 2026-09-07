#include "reader_file.h"
#include "epub_document.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace reader;
namespace {

const unsigned char legacy_v1_footer[] =
    "\n[GBAR-SAVE:1;O= 0000123456;S=1;T=1;B=1;C=59AAEAA4                                             \n";
static_assert(sizeof(legacy_v1_footer) == TXT_SAVE_FOOTER_V1_SIZE + 1);

class FileSource final : public ByteSource {
public:
    explicit FileSource(const char* path) : _file(std::fopen(path, "rb")), _size(0) {
        assert(_file); assert(std::fseek(_file, 0, SEEK_END) == 0);
        _size = uint32_t(std::ftell(_file));
    }
    ~FileSource() override { std::fclose(_file); }
    uint32_t size() const override { return _size; }
    bool byte_at(uint32_t offset, unsigned char& value) const override {
        return offset < _size && std::fseek(_file, long(offset), SEEK_SET) == 0 &&
               std::fread(&value, 1, 1, _file) == 1;
    }
private:
    mutable std::FILE* _file;
    uint32_t _size;
};

void test_public_name_and_footer_helpers()
{
    assert(supported_book_name("BOOK.TXT"));
    assert(supported_book_name("Book.EPUB"));
    assert(!supported_book_name("a.epub.zip"));
    assert(!supported_book_name(".epub"));

    TxtSaveFooter saved{}; saved.byte_offset = 1234; saved.settings = {2, 3, 4};
    unsigned char v3[TXT_SAVE_FOOTER_SIZE]; make_txt_save_footer(saved, v3);
    uint32_t logical = 0, footer = 0; bool valid = false;
    assert(book_size_without_footer("book.txt", 5000, v3, sizeof(v3), logical, valid, footer));
    assert(valid && footer == TXT_SAVE_FOOTER_SIZE && logical == 5000 - TXT_SAVE_FOOTER_SIZE);
    assert(book_size_without_footer("book.txt", 5000, legacy_v1_footer,
                                    TXT_SAVE_FOOTER_V1_SIZE, logical, valid, footer));
    assert(valid && footer == TXT_SAVE_FOOTER_V1_SIZE && logical == 5000 - TXT_SAVE_FOOTER_V1_SIZE);
    v3[100] ^= 1;
    assert(book_size_without_footer("book.txt", 5000, v3, sizeof(v3), logical, valid, footer));
    assert(!valid && footer == TXT_SAVE_FOOTER_SIZE);
}

void test_footer_transaction_recovery()
{
    const auto partial = footer_write_transaction_for_tests(TXT_SAVE_FOOTER_V1_SIZE, 17, false);
    assert(!partial.success && partial.old_footer_restored);
    assert(partial.physical_size == 8 + TXT_SAVE_FOOTER_V1_SIZE);
    assert(partial.old_footer_parseable);
    const auto v2_partial = footer_write_transaction_for_tests(TXT_SAVE_FOOTER_V2_SIZE, 17, false);
    assert(!v2_partial.success && v2_partial.old_footer_restored);
    assert(v2_partial.physical_size == 8 + TXT_SAVE_FOOTER_V2_SIZE);
    assert(v2_partial.old_footer_parseable);
    const auto sync = footer_write_transaction_for_tests(
            TXT_SAVE_FOOTER_SIZE, TXT_SAVE_FOOTER_SIZE, true);
    assert(!sync.success && sync.old_footer_restored);
    assert(sync.physical_size == 8 + TXT_SAVE_FOOTER_SIZE);
    assert(sync.old_footer_parseable);
}

void test_legacy_v047_epub_is_exposed_as_original_archive(const char* path, const char* migrated_path)
{
    FileSource physical(path);
    unsigned char tail[TXT_SAVE_FOOTER_V2_SIZE + 32]{};
    assert(physical.size() >= sizeof(tail));
    assert(physical.read_range(physical.size() - sizeof(tail), tail, sizeof(tail)));
    BookStorageLayout layout{};
    assert(inspect_book_tail("legacy-v047.epub", physical.size(), tail, sizeof(tail), layout));
    assert(layout.has_valid_footer && layout.footer_size == TXT_SAVE_FOOTER_V2_SIZE);
    assert(layout.book_size < physical.size());
    TxtSaveFooter saved{};
    assert(parse_txt_save_footer(tail + 32, TXT_SAVE_FOOTER_V2_SIZE, saved));
    assert(saved.byte_offset == 42);
    class PrefixSource final : public ByteSource {
    public:
        PrefixSource(const ByteSource& input, uint32_t length) : _input(input), _length(length) {}
        uint32_t size() const override { return _length; }
        bool byte_at(uint32_t offset, unsigned char& value) const override {
            return offset < _length && _input.byte_at(offset, value);
        }
    private:
        const ByteSource& _input; uint32_t _length;
    } archive(physical, layout.book_size);
    EpubDocument document;
    assert(document.open(archive));
    assert(write_epub_cache_file_for_tests(path, migrated_path, document));
    FileSource migrated(migrated_path); EpubDocument migrated_document;
    assert(migrated_document.open(migrated));
    assert(migrated_document.optimized_size() == document.size());
}

void test_valid_zip_cache_member_and_fallback(const char* input, const char* output)
{
    FileSource original(input); EpubDocument normalized; assert(normalized.open(original));
    assert(write_epub_cache_file_for_tests(input, output, normalized));
    FileSource cached(output); EpubDocument loaded; assert(loaded.open(cached));
    assert(loaded.optimized_size() == normalized.size());
    assert(corrupt_epub_cache_file_for_tests(output));
    FileSource corrupt(output); EpubDocument fallback; assert(fallback.open(corrupt));
    assert(!fallback.optimized_size() && fallback.size() == normalized.size());
    assert(write_epub_cache_file_for_tests(input, output, normalized));
}
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    test_public_name_and_footer_helpers();
    test_footer_transaction_recovery();
    test_legacy_v047_epub_is_exposed_as_original_archive(argv[3], argv[4]);
    test_valid_zip_cache_member_and_fallback(argv[1], argv[2]);
    std::puts("PASS: ReaderFile ZIP cache helpers");
}
