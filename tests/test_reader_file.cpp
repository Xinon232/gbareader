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
    assert(fallback.open(corrupt_source));
    unsigned char recovered_byte;
    assert(fallback.byte_at(0, recovered_byte) && !fallback.optimized_size());
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

uint32_t reference_crc(const unsigned char* bytes, uint32_t count)
{
    uint32_t c = 0xffffffffu;
    for(uint32_t i = 0; i < count; ++i) {
        c ^= bytes[i];
        for(int bit = 0; bit < 8; ++bit) c = (c & 1) ? (c >> 1) ^ 0xedb88320u : c >> 1;
    }
    return ~c;
}
void set32(std::vector<unsigned char>& raw, uint32_t at, uint32_t value)
{
    for(int i = 0; i < 4; ++i) raw[at + i] = static_cast<unsigned char>(value >> (8 * i));
}
uint32_t entry_data(const std::vector<unsigned char>& raw, uint32_t entry)
{
    const auto local = zip32(raw, entry + 42);
    return local + 30 + zip16(raw, local + 26) + zip16(raw, local + 28);
}
class PayloadSource final : public ByteSource {
public:
    struct Span { uint32_t at, size; };
    const std::vector<unsigned char>& raw;
    std::vector<Span> chapters;
    mutable uint32_t payload_reads = 0, byte_calls = 0, block_calls = 0, cache_bytes = 0;
    mutable uint32_t window_switches = 0, last_window = 0xffffffffu;
    uint32_t cache_at = 0, cache_size = 0, table_at = 0;
    mutable uint32_t table_reads = 0;
    bool deny_chapters = false, fail_cache = false, short_text = false;
    explicit PayloadSource(const std::vector<unsigned char>& bytes) : raw(bytes) {
        uint32_t at = zip32(raw, raw.size() - 6);
        for(int i = 0; i < zip16(raw, raw.size() - 12); ++i) {
            const auto n = zip16(raw, at + 28);
            const auto* name = raw.data() + at + 46;
            if(n >= 6 && !std::memcmp(name + n - 6, ".xhtml", 6))
                chapters.push_back({entry_data(raw, at), zip32(raw, at + 20)});
            if(n == std::strlen("META-INF/gbareader/cache-v5") &&
               !std::memcmp(name, "META-INF/gbareader/cache-v5", n)) {
                cache_at = entry_data(raw, at) + 32; cache_size = zip32(raw, at + 24) - 32;
                table_at = cache_at + zip32(raw, cache_at - 16);
            }
            at += 46 + n + zip16(raw, at + 30) + zip16(raw, at + 32);
        }
        assert(!chapters.empty());
    }
    uint32_t size() const override { return raw.size(); }
    bool byte_at(uint32_t at, unsigned char& value) const override {
        ++byte_calls; return read_range(at, &value, 1);
    }
    bool read_range(uint32_t at, unsigned char* out, uint32_t count) const override {
        ++block_calls;
        if(count) for(uint32_t w = at / 8192; w <= (at + count - 1) / 8192; ++w) {
            if(w != last_window) { ++window_switches; last_window = w; }
        }
        if(!out || at > size() || count > size() - at) return false;
        for(auto chapter : chapters) if(count && at < chapter.at + chapter.size && chapter.at < at + count) {
            ++payload_reads; if(deny_chapters) return false;
        }
        if(count && table_at && at < cache_at + cache_size && table_at < at + count) ++table_reads;
        if(count && cache_size && at < cache_at + cache_size && cache_at < at + count) {
            if(fail_cache) return false;
            if(short_text && at < table_at) {
                std::memcpy(out, raw.data()+at, count/2); return false;
            }
            const auto begin = at > cache_at ? at : cache_at;
            const auto end = at + count < cache_at + cache_size ? at + count : cache_at + cache_size;
            cache_bytes += end - begin;
        }
        std::memcpy(out, raw.data() + at, count); return true;
    }
};

void test_bounded_metadata_lookup(const char* input)
{
    const char* slash = std::strrchr(input, '/'); assert(slash);
    char path[1024]; const auto prefix = size_t(slash - input + 1);
    std::memcpy(path, input, prefix); std::strcpy(path + prefix, "hundred-spine-items.epub");
    FileSource file(path); std::vector<unsigned char> raw(file.size());
    assert(file.read_range(0, raw.data(), raw.size()));
    const auto original = raw; MemorySource src(original.data(), original.size());
    EpubDocument text; assert(text.open(src));
    TxtSaveFooter footer{}; footer.settings = default_settings();
    assert(append_epub_transaction_for_tests(raw, &text, footer).success);
    PayloadSource measured(raw); measured.deny_chapters = true;
    EpubDocument cached; assert(cached.open(measured) && cached.optimized_size());
    std::printf("METADATA calls=%u 8KiB_window_switches=%u\n", measured.block_calls, measured.window_switches);
    std::fflush(stdout);
    assert(measured.block_calls < 7000);
    assert(measured.window_switches < 400);
    assert(!measured.payload_reads);
    std::strcpy(path + prefix, "nul-required-name.epub");
    FileSource nul(path); EpubDocument invalid;
    assert(!invalid.open(nul) && invalid.error() == EpubError::MISSING_MANIFEST_ITEM);
}

void test_lazy_block_cache(const char* input)
{
    const char* slash = std::strrchr(input, '/'); assert(slash);
    char path[1024]; const auto prefix = size_t(slash - input + 1);
    std::memcpy(path, input, prefix); std::strcpy(path + prefix, "large-streamed.epub");
    FileSource file(path); std::vector<unsigned char> raw(file.size());
    assert(file.read_range(0, raw.data(), raw.size()));
    const auto original = raw; MemorySource src(original.data(), original.size());
    EpubDocument text; assert(text.open(src));
    TxtSaveFooter footer{}; footer.settings = default_settings(); footer.byte_offset = 17001;
    assert(append_epub_transaction_for_tests(raw, &text, footer).success);
    PayloadSource measured(raw); measured.deny_chapters = true;
    EpubDocument cached; assert(cached.open(measured) && cached.optimized_size());
    std::printf("CACHE startup text=%u cache_bytes=%u calls=%u\n", text.size(), measured.cache_bytes, measured.block_calls);
    std::fflush(stdout);
    assert(measured.cache_bytes < text.size() / 16);
    assert(measured.block_calls < 400);
    measured.table_reads = 0;
    unsigned char page[700], expected[700];
    for(uint32_t at : {17001u, 4090u, 524280u, text.size() - 700u, 0u}) {
        const auto before = measured.cache_bytes;
        assert(cached.read_range(at, page, sizeof(page)));
        assert(text.read_range(at, expected, sizeof(expected)));
        assert(!std::memcmp(page, expected, sizeof(page)));
        assert(measured.cache_bytes - before <= 8192 + 512);
        if(at == 4090) assert(measured.table_reads == 1);
    }
    assert(measured.payload_reads == 0);
    assert(measured.table_reads == 5); // 0, 1, 9, 10 (partial final block), then 0 again
    // An unopened bad final block must not spoil startup or a different page.
    int matches; const auto cache = live_entry(raw, "META-INF/gbareader/cache-v5", matches);
    const auto header = entry_data(raw, cache);
    const auto good = raw;
    raw[header + 32 + text.size() - 1] ^= 1;
    PayloadSource late(raw); EpubDocument fallback;
    assert(fallback.open(late) && fallback.optimized_size());
    assert(late.payload_reads == 0);
    assert(fallback.read_range(0, page, sizeof(page)) && fallback.optimized_size());
    unsigned char last = 0xa5;
    assert(fallback.byte_at(text.size()-1, last) && last == '\n');
    assert(!fallback.optimized_size() && fallback.size() == text.size() && late.payload_reads > 0);
    assert(append_epub_transaction_for_tests(raw, &fallback, footer).success);
    PayloadSource rebuilt(raw); EpubDocument repaired;
    assert(repaired.open(rebuilt) && repaired.optimized_size());
    assert(repaired.byte_at(repaired.size()-1, last) && last == '\n');
    assert(!repaired.export_text([](void*,const unsigned char*,uint32_t){return false;},nullptr));
    assert(repaired.read_range(4090,page,sizeof(page)));
    assert(text.read_range(4090,expected,sizeof(expected)) && !std::memcmp(page,expected,sizeof(page)));

    raw = good; raw[header+32+text.size()-1] ^= 1;
    raw[measured.chapters.front().at] ^= 1;
    PayloadSource both_bad(raw); EpubDocument unsafe;
    assert(unsafe.open(both_bad) && unsafe.optimized_size());
    last = 0xa5;
    assert(!unsafe.byte_at(unsafe.size()-1,last) && last == 0xa5 && unsafe.size() == 0);
    assert(unsafe.error() == EpubError::MALFORMED_ZIP);
    raw = good;
    PayloadSource read_failed(raw); EpubDocument io_fallback;
    assert(io_fallback.open(read_failed) && io_fallback.optimized_size());
    read_failed.fail_cache = true; // failure on the first demanded table/block
    assert(io_fallback.byte_at(0,last) && !io_fallback.optimized_size());
    assert(text.byte_at(0,expected[0]) && last == expected[0]);
    PayloadSource short_source(raw); EpubDocument short_fallback;
    assert(short_fallback.open(short_source) && short_fallback.optimized_size());
    short_source.short_text = true;
    assert(short_fallback.byte_at(0,last) && !short_fallback.optimized_size() && last == expected[0]);
    assert(short_source.payload_reads > 0);

    // Legacy owned v5 caches remain fully checked until the next user save.
    raw = good;
    const auto local = zip32(raw, cache + 42);
    raw[header + 8] = 5;
    set32(raw, header + 24, zip32(raw, header + 20));
    set32(raw, header + 28, reference_crc(raw.data()+header,28));
    const auto legacy_size = 32 + text.size();
    const auto whole = reference_crc(raw.data()+header,legacy_size);
    set32(raw, local+14, whole); set32(raw, cache+16, whole);
    set32(raw, local+18, legacy_size); set32(raw, local+22, legacy_size);
    set32(raw, cache+20, legacy_size); set32(raw, cache+24, legacy_size);
    PayloadSource legacy(raw); EpubDocument old;
    assert(old.open(legacy) && old.optimized_size());
    assert(legacy.cache_bytes == text.size() && legacy.payload_reads == 0);
    uint32_t central, bytes; uint16_t entries;
    assert(old.cache_archive_layout(central, bytes, entries));
    raw[header + 32 + text.size()-1] ^= 1;
    PayloadSource bad_legacy(raw); EpubDocument rejected;
    assert(rejected.open(bad_legacy) && !rejected.optimized_size() && bad_legacy.payload_reads > 0);
}

void test_utf8_boundary_source_invalidation(const char* input)
{
    const char* slash = std::strrchr(input, '/'); assert(slash);
    char path[1024]; const auto prefix = size_t(slash - input + 1);
    assert(prefix + 64 < sizeof(path));
    std::memcpy(path, input, prefix);
    enum Failure { CORRUPT_ORIGINAL, READ_FAILURE, LENGTH_MISMATCH };
    const char* codepoints[] = {u8"£", u8"€", u8"🙂"};
    int cases = 0, failures = 0;
    for(const char* codepoint : codepoints) {
        const auto width = uint32_t(std::strlen(codepoint));
        for(uint32_t split = 1; split < width; ++split) {
            std::snprintf(path + prefix, sizeof(path) - prefix,
                          "utf8-boundary-%u-%u.epub", width, split);
            FileSource file(path); std::vector<unsigned char> original(file.size());
            assert(file.read_range(0, original.data(), original.size()));
            MemorySource original_source(original.data(), original.size());
            EpubDocument normalized; assert(normalized.open(original_source));
            const uint32_t start = 4096 - split;
            unsigned char bytes[4];
            assert(normalized.read_range(start, bytes, width));
            assert(!std::memcmp(bytes, codepoint, width));
            Page expected{};
            assert(layout_page(normalized, start, default_settings(), nullptr, expected));
            assert(expected.start_offset == start && expected.next_offset == normalized.size());
            assert(expected.eof && expected.line_count == 1);
            assert(!std::memcmp(expected.lines[0].text, codepoint, width));
            assert(!std::strcmp(expected.lines[0].text + width, "tail"));

            for(auto failure : {CORRUPT_ORIGINAL, READ_FAILURE, LENGTH_MISMATCH}) {
                auto raw = original;
                TxtSaveFooter footer{}; footer.settings = default_settings();
                std::vector<unsigned char> changed(normalized.size() + 1);
                assert(normalized.read_range(0, changed.data(), normalized.size()));
                changed.back() = 'x';
                MemorySource wrong_length(changed.data(), changed.size());
                const ByteSource& cache_text = failure == LENGTH_MISMATCH ?
                    static_cast<const ByteSource&>(wrong_length) : normalized;
                assert(append_epub_transaction_for_tests(raw, &cache_text, footer).success);
                int matches; const auto cache = live_entry(raw, "META-INF/gbareader/cache-v5", matches);
                assert(matches == 1);
                // Corruption is in the next block, never in the already readable prefix.
                if(failure != READ_FAILURE) raw[entry_data(raw, cache) + 32 + 4096] ^= 1;
                if(failure == CORRUPT_ORIGINAL) {
                    const auto chapter = live_entry(raw, "OEBPS/chapter.xhtml", matches);
                    assert(matches == 1);
                    raw[entry_data(raw, chapter) + 20] ^= 1; // deliberately stale ZIP CRC
                }
                for(bool open : {false, true}) {
                    PayloadSource source(raw); EpubDocument document;
                    assert(document.open(source) && document.optimized_size() == cache_text.size());
                    assert(source.payload_reads == 0);
                    assert(document.read_range(start, bytes, split)); // prime only block zero
                    assert(!std::memcmp(bytes, codepoint, split) && source.payload_reads == 0);
                    if(failure == READ_FAILURE) {
                        source.fail_cache = true;
                        source.deny_chapters = true;
                    }
                    // A real caller's existing page must survive the failed operation byte-for-byte.
                    Page page{};
                    std::memcpy(&page, &expected, sizeof(page));
                    unsigned char before[sizeof(Page)];
                    std::memcpy(before, &page, sizeof(page));
                    PageHistory history{};
                    const bool success = open ?
                        open_page_at(document, start, default_settings(), nullptr, history, page) :
                        layout_page(document, start, default_settings(), nullptr, page);
                    assert(document.size() == 0 && source.payload_reads > 0);
                    assert(document.error() == (failure == READ_FAILURE ?
                           EpubError::READ_FAILED : EpubError::MALFORMED_ZIP));
                    const bool unchanged = !std::memcmp(before, &page, sizeof(page));
                    ++cases;
                    if(success || !unchanged) {
                        ++failures;
                        std::printf("FAIL UTF8 width=%u split=%u failure=%d open=%d success=%d unchanged=%d\n",
                                    width, split, int(failure), open, success, unchanged);
                    }
                }
            }
        }
    }
    std::printf("UTF8 boundary invalidation: cases=%d failures=%d\n", cases, failures);
    std::fflush(stdout);
    assert(cases == 36 && failures == 0);
}

void test_late_cache_length_mismatch(const char* input)
{
    FileSource file(input); std::vector<unsigned char> raw(file.size());
    assert(file.read_range(0, raw.data(), raw.size()));
    const auto original = raw; MemorySource src(original.data(), original.size());
    EpubDocument original_text; assert(original_text.open(src));
    std::vector<unsigned char> changed(original_text.size() + 1, 'x');
    MemorySource wrong_length(changed.data(), changed.size());
    TxtSaveFooter footer{}; footer.settings = default_settings();
    assert(append_epub_transaction_for_tests(raw, &wrong_length, footer).success);
    int matches; const auto cache = live_entry(raw, "META-INF/gbareader/cache-v5", matches);
    raw[entry_data(raw, cache) + 32] ^= 1;
    PayloadSource source(raw); EpubDocument cached;
    assert(cached.open(source) && cached.optimized_size());
    unsigned char untouched = 0xa5;
    assert(!cached.byte_at(0, untouched));
    assert(untouched == 0xa5 && cached.size() == 0 && cached.error() == EpubError::MALFORMED_ZIP);
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
    assert(matches == 1);
    const auto local = zip32(raw, cache + 42);
    const auto header = entry_data(raw, cache);
    PayloadSource valid(raw);
    valid.deny_chapters = true;
    EpubDocument cached;
    assert(cached.open(valid) && cached.optimized_size() == text.size());
    assert(valid.payload_reads == 0 && valid.byte_calls == 0 && valid.block_calls > 0);
    assert(valid.cache_bytes == (text.size() / 4096 + (text.size() % 4096 != 0)) * 4); // table only
    unsigned char value;
    assert(cached.byte_at(0, value));
    assert(cached.byte_at(cached.size() - 1, value) && value == '\n');
    assert(valid.payload_reads == 0);

    // Deliberate policy: intact metadata/cache defers original payload integrity.
    raw[valid.chapters.front().at] ^= 1;
    PayloadSource corrupt_original(raw);
    EpubDocument deferred;
    assert(deferred.open(corrupt_original) && deferred.optimized_size() == text.size());
    assert(corrupt_original.payload_reads == 0);

    enum Corruption { MAGIC, CACHE_VERSION, TEXT_VERSION, FINGERPRINT, LENGTH,
                      HEADER_CRC, TEXT_CRC, TEXT_PAYLOAD, TABLE_CRC, TABLE_PAYLOAD, WHOLE_CRC };
    for(auto corruption : {MAGIC, CACHE_VERSION, TEXT_VERSION, FINGERPRINT, LENGTH,
                           HEADER_CRC, TEXT_CRC, TEXT_PAYLOAD, TABLE_CRC, TABLE_PAYLOAD, WHOLE_CRC}) {
        for(bool corrupt_chapter : {false, true}) {
            raw = good;
            switch(corruption) {
            case MAGIC: raw[header] ^= 1; break;
            case CACHE_VERSION: raw[header + 8] ^= 1; break;
            case TEXT_VERSION: raw[header + 10] ^= 1; break;
            case FINGERPRINT: raw[header + 12] ^= 1; break;
            case LENGTH: raw[header + 16] ^= 1; break;
            case HEADER_CRC: raw[header + 28] ^= 1; break;
            case TEXT_CRC: raw[header + 20] ^= 1; break;
            case TEXT_PAYLOAD: raw[header + 32 + text.size() - 1] ^= 1; break;
            case TABLE_CRC: raw[header + 24] ^= 1; break;
            case TABLE_PAYLOAD:
                raw[header + 32 + text.size()] ^= 1;
                set32(raw, header + 24, reference_crc(raw.data()+header+32+text.size(),
                      zip32(raw, cache+24)-32-text.size()));
                break;
            case WHOLE_CRC: break;
            }
            // Repair independent outer checks to isolate each guard, not just
            // make all mutations fail at the first checksum comparison.
            if(corruption != HEADER_CRC) set32(raw, header + 28, reference_crc(raw.data() + header, 28));
            auto whole = reference_crc(raw.data() + header, zip32(raw, cache + 24));
            if(corruption == WHOLE_CRC) whole ^= 1;
            set32(raw, local + 14, whole); set32(raw, cache + 16, whole);
            if(corrupt_chapter) raw[valid.chapters.front().at] ^= 1;
            PayloadSource bad(raw);
            EpubDocument fallback;
            if(corrupt_chapter) {
                assert(!fallback.open(bad) && fallback.error() == EpubError::MALFORMED_ZIP);
                assert(fallback.size() == 0);
            } else {
                assert(fallback.open(bad) && !fallback.optimized_size());
                assert(fallback.size() == text.size());
            }
            assert(bad.payload_reads > 0);
        }
    }
    raw = good;
    PayloadSource failed_cache(raw); failed_cache.fail_cache = true;
    EpubDocument fallback;
    assert(fallback.open(failed_cache) && !fallback.optimized_size());
    assert(failed_cache.payload_reads > 0);
    raw[valid.chapters.front().at] ^= 1;
    assert(!fallback.open(failed_cache) && fallback.error() == EpubError::MALFORMED_ZIP);

    PayloadSource missing(original);
    assert(fallback.open(missing) && !fallback.optimized_size());
    assert(missing.payload_reads > 0);

    // Local/central package safety is never deferred, even with a valid cache.
    raw = good;
    const auto chapter_local = valid.chapters.front().at; // preceding payload boundary
    for(uint32_t at = 0; at + 30 < chapter_local; ++at) {
        if(zip32(raw, at) == 0x04034b50u &&
           at + 30 + zip16(raw, at + 26) + zip16(raw, at + 28) == chapter_local) {
            raw[at + 14] ^= 1; break;
        }
    }
    PayloadSource bad_metadata(raw);
    assert(!fallback.open(bad_metadata) && fallback.error() == EpubError::MALFORMED_ZIP);
    assert(bad_metadata.payload_reads == 0);
}

void test_cached_required_metadata_guards(const char* input)
{
    const char* slash = std::strrchr(input, '/');
    assert(slash);
    struct Case { const char* file; EpubError error; };
    const Case cases[] = {
        {"required-image-suffix-zip64.epub", EpubError::ZIP64},
        {"required-image-suffix-local-crc.epub", EpubError::MALFORMED_ZIP},
        {"unsupported.epub", EpubError::UNSUPPORTED_COMPRESSION},
        {"declared-large.epub", EpubError::CHAPTER_TOO_LARGE},
        {"compressed-entry-too-large.epub", EpubError::COMPRESSED_ENTRY_TOO_LARGE},
        {"payload-overlap.epub", EpubError::MALFORMED_ZIP},
        {"duplicate-required-name.epub", EpubError::MALFORMED_ZIP},
        {"local-flags-mismatch.epub", EpubError::MALFORMED_ZIP},
        {"descriptor-bad-crc.epub", EpubError::MALFORMED_ZIP},
        {"metadata-too-large.epub", EpubError::METADATA_TOO_LARGE},
        {"missing-manifest.epub", EpubError::MISSING_MANIFEST_ITEM},
        {"too-many-spine-items.epub", EpubError::TOO_MANY_SPINE_ITEMS}
    };
    const unsigned char normalized[] = "Valid cached text.\n";
    MemorySource text(normalized, sizeof(normalized) - 1);
    TxtSaveFooter footer{}; footer.settings = default_settings();
    for(const auto& test : cases) {
        char path[1024];
        std::snprintf(path, sizeof(path), "%.*s%s", int(slash - input + 1), input, test.file);
        FileSource archive(path);
        std::vector<unsigned char> raw(archive.size());
        assert(archive.read_range(0, raw.data(), raw.size()));
        // The production writer gives the crafted original metadata a matching
        // fingerprint and valid header/text/member CRCs. Cache trust must not
        // replace required-entry structural checks, including image suffixes.
        assert(append_epub_transaction_for_tests(raw, &text, footer).success);
        if(!std::strcmp(test.file, "payload-overlap.epub")) {
            // Appending moved the live directory: extend the declared stored
            // payload to cross the NEW boundary, not the obsolete directory.
            int matches;
            const auto chapter = live_entry(raw, "OEBPS/chapter.xhtml", matches);
            assert(matches == 1);
            const auto local = zip32(raw, chapter + 42);
            const auto directory = zip32(raw, raw.size() - 6);
            const auto length = directory + 1 - entry_data(raw, chapter);
            set32(raw, chapter + 20, length); set32(raw, chapter + 24, length);
            set32(raw, local + 18, length); set32(raw, local + 22, length);
            std::vector<unsigned char> records;
            for(uint32_t at = directory; at < raw.size() - 22;) {
                const auto n = zip16(raw, at + 28);
                const auto size = 46u + n + zip16(raw, at + 30) + zip16(raw, at + 32);
                if(n < 19 || std::memcmp(raw.data() + at + 46, "META-INF/gbareader/", 19))
                    records.insert(records.end(), raw.begin() + at, raw.begin() + at + size);
                at += size;
            }
            const auto cache = live_entry(raw, "META-INF/gbareader/cache-v5", matches);
            assert(matches == 1);
            const auto header = entry_data(raw, cache);
            set32(raw, header + 12, reference_crc(records.data(), records.size()));
            set32(raw, header + 28, reference_crc(raw.data() + header, 28));
            const auto whole = reference_crc(raw.data() + header, zip32(raw, cache + 24));
            set32(raw, cache + 16, whole); set32(raw, zip32(raw, cache + 42) + 14, whole);
        }
        MemorySource source(raw.data(), raw.size());
        EpubDocument rejected;
        const bool opened = rejected.open(source);
        if(opened) std::fprintf(stderr, "Unexpected cached metadata acceptance: %s\n", test.file);
        assert(!opened);
        if(rejected.error() != test.error)
            std::fprintf(stderr, "%s: %s\n", test.file, epub_error_string(rejected.error()));
        assert(rejected.error() == test.error && rejected.size() == 0);
    }
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
    assert(fallback.open(saved) && fallback.optimized_size());
    unsigned char untouched = 0xA5;
    assert(!fallback.byte_at(0, untouched) && untouched == 0xA5 && fallback.error() == EpubError::INVALID_XHTML);
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
    unsigned char recovered; assert(fallback.byte_at(0, recovered));
    assert(!fallback.optimized_size() && fallback.size() == normalized.size());
    assert(write_epub_cache_file_for_tests(input, output, normalized));
}
}

int main(int argc, char** argv)
{
    assert(argc == 10);
    test_utf8_boundary_source_invalidation(argv[1]);
    test_late_cache_length_mismatch(argv[1]);
    test_bounded_metadata_lookup(argv[1]);
    test_lazy_block_cache(argv[1]);
    test_cached_required_metadata_guards(argv[1]);
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
