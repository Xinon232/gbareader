#include "reader_file.h"
#include "epub_document.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

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

uint16_t zip16(const std::vector<unsigned char>& raw, size_t at)
{
    assert(at + 2 <= raw.size());
    return uint16_t(raw[at] | uint16_t(raw[at + 1]) << 8);
}
uint32_t zip32(const std::vector<unsigned char>& raw, size_t at)
{
    return uint32_t(zip16(raw, at)) | uint32_t(zip16(raw, at + 2)) << 16;
}
uint32_t live_entry(const std::vector<unsigned char>& raw, const char* name, int& matches)
{
    assert(zip32(raw, raw.size() - 22) == 0x06054b50u);
    uint32_t at = zip32(raw, raw.size() - 6), found = 0;
    const int count = zip16(raw, raw.size() - 12);
    matches = 0;
    for(int i = 0; i < count; ++i) {
        assert(zip32(raw, at) == 0x02014b50u);
        const auto length = zip16(raw, at + 28);
        if(length == std::strlen(name) && !std::memcmp(raw.data() + at + 46, name, length)) {
            ++matches; found = at;
        }
        at += 46u + length + zip16(raw, at + 30) + zip16(raw, at + 32);
    }
    assert(at == raw.size() - 22);
    return found;
}

void test_repeated_state_and_cache_regeneration(const char* input, const char* output)
{
    FileSource archive(input);
    std::vector<unsigned char> raw(archive.size());
    assert(archive.read_range(0, raw.data(), raw.size()));
    const auto original = raw;
    MemorySource source(original.data(), original.size());
    EpubDocument text;
    assert(text.open(source));
    TxtSaveFooter footer{}; footer.settings = default_settings();
    assert(append_epub_transaction_for_tests(raw, &text, footer).success);
    const auto count = zip16(raw, raw.size() - 12);
    for(uint32_t i = 1; i <= 32; ++i) {
        footer.byte_offset = i;
        assert(append_epub_transaction_for_tests(raw, nullptr, footer).success);
        assert(zip16(raw, raw.size() - 12) == count);
        int matches;
        const auto state = live_entry(raw, "META-INF/gbareader/state-v5", matches);
        assert(matches == 1);
        const auto local = zip32(raw, state + 42);
        const auto data = local + 30 + zip16(raw, local + 26) + zip16(raw, local + 28);
        TxtSaveFooter loaded{};
        assert(parse_txt_save_footer(raw.data() + data, TXT_SAVE_FOOTER_SIZE, loaded));
        assert(loaded.byte_offset == i);
    }
    assert(!std::memcmp(raw.data(), original.data(), original.size()));
    int matches;
    auto cache = live_entry(raw, "META-INF/gbareader/cache-v5", matches);
    assert(matches == 1);
    auto local = zip32(raw, cache + 42);
    raw[local + 30 + zip16(raw, local + 26) + zip16(raw, local + 28) + 32] ^= 1;
    // Keep the source stable across vector reallocations made by the writer.
    const auto corrupt = raw;
    MemorySource corrupt_source(corrupt.data(), corrupt.size());
    EpubDocument fallback;
    assert(fallback.open(corrupt_source) && !fallback.optimized_size());
    assert(append_epub_transaction_for_tests(raw, &fallback, footer).success);
    live_entry(raw, "META-INF/gbareader/cache-v5", matches);
    assert(matches == 1 && zip16(raw, raw.size() - 12) == count);
    MemorySource regenerated(raw.data(), raw.size());
    EpubDocument reopened;
    assert(reopened.open(regenerated) && reopened.optimized_size() == text.size());
    auto* file = std::fopen(output, "wb");
    assert(file && std::fwrite(raw.data(), 1, raw.size(), file) == raw.size());
    assert(std::fclose(file) == 0);
}

void test_append_size_preflight(const char* input)
{
    FileSource file(input);
    std::vector<unsigned char> raw(file.size());
    assert(file.read_range(0, raw.data(), raw.size()));
    const auto original = raw;
    const unsigned char bytes[] = "text\n";
    MemorySource text(bytes, sizeof(bytes) - 1);
    TxtSaveFooter footer{}; footer.settings = default_settings();
    for(bool cache : {true, false}) {
        // The harness advertises a physical append offset without allocating
        // its gap. A rejected oversized save must not seek/write any payload.
        auto result = append_epub_transaction_for_tests(raw, cache ? &text : nullptr, footer,
                EpubTestFault::NONE, 0, EPUB_MAX_ARCHIVE_BYTES - 1);
        assert(!result.success);
        assert(result.calls[int(EpubTestFault::WRITE)] == 0);
        assert(raw == original);
    }
    class OversizedText final : public ByteSource {
    public:
        uint32_t size() const override { return EPUB_MAX_ARCHIVE_BYTES; }
        bool byte_at(uint32_t, unsigned char&) const override { return false; }
        bool export_text(TextSink, void*) const override { exported = true; return false; }
        mutable bool exported = false;
    } large;
    assert(!append_epub_transaction_for_tests(raw, &large, footer).success);
    assert(!large.exported && raw == original);
}

void test_cache_integrity_guards(const char* input)
{
    FileSource file(input);
    std::vector<unsigned char> raw(file.size());
    assert(file.read_range(0, raw.data(), raw.size()));
    const auto original = raw;
    MemorySource source(original.data(), original.size());
    EpubDocument text;
    assert(text.open(source));
    TxtSaveFooter footer{}; footer.settings = default_settings();
    assert(append_epub_transaction_for_tests(raw, &text, footer).success);
    const auto good = raw;
    int matches;
    const auto cache = live_entry(raw, "META-INF/gbareader/cache-v5", matches);
    const auto local = zip32(raw, cache + 42);
    // Matching but incorrect local/central cache-member CRC is not a valid cache.
    raw[cache + 16] ^= 1;
    raw[local + 14] ^= 1;
    MemorySource bad_crc(raw.data(), raw.size());
    EpubDocument fallback;
    assert(fallback.open(bad_crc) && !fallback.optimized_size());
    assert(fallback.size() == text.size());
    // The original payload must still pass its CRC/deflate checks, even though
    // its untouched metadata continues to match the owned cache fingerprint.
    raw = good;
    const char* chapter = "OEBPS/two.xhtml";
    auto entry = live_entry(raw, chapter, matches);
    if(!matches) { chapter = "OEBPS/chapter.xhtml"; entry = live_entry(raw, chapter, matches); }
    assert(matches == 1);
    const auto chapter_local = zip32(raw, entry + 42);
    const auto data = chapter_local + 30 + zip16(raw, chapter_local + 26) + zip16(raw, chapter_local + 28);
    raw[data] ^= 1;
    MemorySource corrupt(raw.data(), raw.size());
    EpubDocument rejected;
    assert(!rejected.open(corrupt) && rejected.error() == EpubError::MALFORMED_ZIP);
}

void test_export_and_rollback_failures(const char* input)
{
    FileSource file(input);
    std::vector<unsigned char> original(file.size());
    assert(file.read_range(0, original.data(), original.size()));
    class InterruptedText final : public ByteSource {
    public:
        uint32_t size() const override { return 1024; }
        bool byte_at(uint32_t, unsigned char&) const override { return false; }
        bool export_text(TextSink sink, void* context) const override {
            unsigned char block[512]{};
            if(!sink(context, block, sizeof(block))) return false;
            return false; // Source/parser failure after a successfully written chunk.
        }
    } interrupted;
    TxtSaveFooter footer{}; footer.settings = default_settings();
    auto recovered = original;
    assert(!append_epub_transaction_for_tests(recovered, &interrupted, footer).success);
    assert(recovered == original);
    for(EpubTestFault fault : {EpubTestFault::SEEK, EpubTestFault::TRUNCATE, EpubTestFault::SYNC}) {
        auto failed = original;
        const int nth = fault == EpubTestFault::SEEK ? 2 : 1;
        auto result = append_epub_transaction_for_tests(failed, &interrupted, footer, fault, nth);
        assert(!result.success && result.fault_hit);
        assert(failed.size() >= original.size());
        assert(!std::memcmp(failed.data(), original.data(), original.size()));
        // A failed rollback seek must not truncate at the stale append position.
        if(fault == EpubTestFault::SEEK) assert(result.calls[int(EpubTestFault::TRUNCATE)] == 0);
    }
}

void test_append_fault_recovery(const char* input)
{
    FileSource file(input);
    std::vector<unsigned char> original(file.size());
    assert(file.read_range(0, original.data(), original.size()));
    MemorySource source(original.data(), original.size());
    EpubDocument text;
    assert(text.open(source));
    TxtSaveFooter footer{}; footer.settings = default_settings();
    for(bool cache : {true, false}) {
        auto baseline = original;
        if(!cache) assert(append_epub_transaction_for_tests(baseline, &text, footer).success);
        auto successful = baseline;
        auto result = append_epub_transaction_for_tests(successful, cache ? &text : nullptr, footer);
        assert(result.success);
        for(int kind = 0; kind < 5; ++kind) {
            assert(result.calls[kind] > 0);
            for(int nth = 1; nth <= result.calls[kind]; ++nth) {
                auto failed = baseline;
                auto failure = append_epub_transaction_for_tests(
                        failed, cache ? &text : nullptr, footer, EpubTestFault(kind), nth);
                assert(!failure.success && failure.fault_hit);
                assert(failed == baseline);
                MemorySource recovered(failed.data(), failed.size());
                EpubDocument reopened;
                assert(reopened.open(recovered) && reopened.size() == text.size());
            }
        }
    }
}

void test_one_pass_cache_transaction(const char* input)
{
    FileSource archive(input);
    std::vector<unsigned char> raw(archive.size());
    assert(archive.read_range(0, raw.data(), raw.size()));
    class ExportOnly final : public ByteSource {
    public:
        uint32_t size() const override { return 40001; }
        bool byte_at(uint32_t, unsigned char&) const override { return false; }
        bool export_text(TextSink sink, void* context) const override {
            assert(++exports == 1);
            unsigned char block[511];
            std::memset(block, 'x', sizeof(block));
            for(uint32_t at = 0; at < size();) {
                uint32_t take = size() - at;
                if(take > sizeof(block)) take = sizeof(block);
                if(!sink(context, block, take)) return false;
                at += take;
            }
            return true;
        }
        mutable int exports = 0;
    } text;
    TxtSaveFooter footer{}; footer.settings = default_settings();
    auto result = append_epub_transaction_for_tests(raw, &text, footer);
    assert(result.success && text.exports == 1);
    MemorySource saved(raw.data(), raw.size());
    EpubDocument cached;
    assert(cached.open(saved) && cached.optimized_size() == text.size());
}

void test_cache_open_skips_normalization(const char* input)
{
    // A valid owned cache may supply normalized text even when the source text
    // is not parseable by TextParser. Package structure and CRC still govern.
    FileSource archive(input);
    std::vector<unsigned char> raw(archive.size());
    assert(archive.read_range(0, raw.data(), raw.size()));
    MemorySource original(raw.data(), raw.size());
    EpubDocument malformed;
    assert(!malformed.open(original) && malformed.error() == EpubError::INVALID_XHTML);
    const unsigned char normalized[] = "Valid normalized cache.\n";
    MemorySource text(normalized, sizeof(normalized) - 1);
    TxtSaveFooter footer{}; footer.settings = default_settings();
    assert(append_epub_transaction_for_tests(raw, &text, footer).success);
    MemorySource saved(raw.data(), raw.size());
    EpubDocument cached;
    assert(cached.open(saved) && cached.optimized_size() == text.size());
    // Corrupting the cache forces the full parser fallback, which rejects it.
    for(size_t i = 0; i + 32 < raw.size(); ++i) {
        if(!std::memcmp(raw.data() + i, "GBAREPC5", 8)) { raw[i + 32] ^= 1; break; }
    }
    EpubDocument fallback;
    assert(!fallback.open(saved) && fallback.error() == EpubError::INVALID_XHTML);
}

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
    std::vector<unsigned char> legacy_bytes(physical.size());
    assert(physical.read_range(0, legacy_bytes.data(), legacy_bytes.size()));
    const auto before = legacy_bytes;
    for(EpubTestFault fault : {EpubTestFault::WRITE, EpubTestFault::SEEK, EpubTestFault::SYNC}) {
        auto failed = before;
        auto result = append_epub_transaction_for_tests(failed, &document, saved, fault, 1);
        assert(!result.success && result.fault_hit && failed == before);
        BookStorageLayout restored{};
        assert(inspect_book_tail("legacy.epub", failed.size(), failed.data() + failed.size() - sizeof(tail),
                                 sizeof(tail), restored));
        assert(restored.has_valid_footer);
        TxtSaveFooter bookmark{};
        assert(parse_txt_save_footer(failed.data() + restored.footer_offset, restored.footer_size, bookmark));
        assert(bookmark.byte_offset == 42);
    }
    assert(append_epub_transaction_for_tests(legacy_bytes, &document, saved).success);
    assert(!std::memcmp(legacy_bytes.data(), before.data(), before.size()));
    MemorySource migrated_bytes(legacy_bytes.data(), legacy_bytes.size());
    EpubDocument migrated_from_transaction;
    assert(migrated_from_transaction.open(migrated_bytes));
    assert(migrated_from_transaction.optimized_size() == document.size());
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
    assert(argc == 10);
    test_repeated_state_and_cache_regeneration(argv[8], argv[9]);
    test_cache_integrity_guards(argv[6]);
    test_cache_integrity_guards(argv[7]);
    test_cache_open_skips_normalization(argv[5]);
    test_one_pass_cache_transaction(argv[1]);
    test_append_fault_recovery(argv[1]);
    test_export_and_rollback_failures(argv[1]);
    test_cache_integrity_guards(argv[1]);
    test_append_size_preflight(argv[1]);
    test_public_name_and_footer_helpers();
    test_footer_transaction_recovery();
    test_legacy_v047_epub_is_exposed_as_original_archive(argv[3], argv[4]);
    test_valid_zip_cache_member_and_fallback(argv[1], argv[2]);
    test_repeated_state_and_cache_regeneration(argv[1], argv[2]);
    std::puts("PASS: ReaderFile ZIP cache helpers");
}
