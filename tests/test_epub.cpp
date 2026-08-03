#include "epub_document.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace reader;

class FileSource final : public ByteSource {
public:
    explicit FileSource(const char* path) : _file(std::fopen(path, "rb")), _size(0) {
        if(_file) { std::fseek(_file, 0, SEEK_END); _size = uint32_t(std::ftell(_file)); }
    }
    ~FileSource() override { if(_file) std::fclose(_file); }
    uint32_t size() const override { return _size; }
    bool byte_at(uint32_t offset, unsigned char& value) const override {
        if(! _file || offset >= _size || std::fseek(_file, long(offset), SEEK_SET)) return false;
        return std::fread(&value, 1, 1, _file) == 1;
    }
private:
    mutable std::FILE* _file;
    uint32_t _size;
};

class FailingSource final : public ByteSource {
public:
    FailingSource(const ByteSource& source, uint32_t fail_at) : _source(source), _fail_at(fail_at) {}
    uint32_t size() const override { return _source.size(); }
    bool byte_at(uint32_t offset, unsigned char& value) const override {
        return offset < _fail_at && _source.byte_at(offset, value);
    }
private:
    const ByteSource& _source;
    uint32_t _fail_at;
};

static void expect_text(const char* path, const char* expected)
{
    FileSource archive(path);
    EpubDocument epub;
    if(! epub.open(archive)) std::fprintf(stderr, "%s: %s\n", path, epub_error_string(epub.error()));
    assert(epub.error() == EpubError::NONE);
    assert(epub.error() == EpubError::NONE);
    if(epub.size() != std::strlen(expected)) std::fprintf(stderr, "%s: size %u expected %zu\n", path, epub.size(), std::strlen(expected));
    assert(epub.size() == std::strlen(expected));
    for(uint32_t i = 0; i < epub.size(); ++i) {
        unsigned char value = 0;
        assert(epub.byte_at(i, value));
        assert(value == static_cast<unsigned char>(expected[i]));
    }
}

static void expect_error(const char* path, EpubError expected)
{
    FileSource archive(path); EpubDocument epub;
    assert(! epub.open(archive));
    if(epub.error() != expected) std::fprintf(stderr, "%s: got %s, expected %s\n", path, epub_error_string(epub.error()), epub_error_string(expected));
    assert(epub.error() == expected); assert(epub.size() == 0);
}

static void expect_repeated_text(const char* path, unsigned char repeated, uint32_t count)
{
    FileSource archive(path);
    EpubDocument epub;
    if(! epub.open(archive)) std::fprintf(stderr, "%s: %s\n", path, epub_error_string(epub.error()));
    assert(epub.error() == EpubError::NONE);
    assert(epub.size() == count + 1);
    for(uint32_t index = 0; index < count; ++index) {
        unsigned char value = 0;
        assert(epub.byte_at(index, value));
        assert(value == repeated);
    }
    unsigned char newline = 0;
    assert(epub.byte_at(count, newline) && newline == '\n');
}

int main(int argc, char** argv)
{
    assert(argc == 49);
    expect_text(argv[1], "Stored chapter.\n");
    expect_text(argv[2], "Deflated chapter.\n");
    expect_text(argv[3], "Second\nA & < > \" '  A A ?\nItem\nFirst file.\n");

    FileSource ordered(argv[3]); EpubDocument book; assert(book.open(ordered));
    const uint32_t boundary = 31; // start of the second spine chapter in the virtual stream.
    unsigned char value = 0; assert(book.byte_at(boundary - 1, value) && value == '\n');
    assert(book.byte_at(boundary, value) && value == 'F');
    PageHistory history{}; Page resumed{};
    assert(open_page_at(book, boundary, default_settings(), nullptr, history, resumed));
    assert(resumed.start_offset == boundary);

    expect_error(argv[4], EpubError::NOT_ZIP);
    expect_error(argv[5], EpubError::MISSING_CONTAINER);
    expect_error(argv[6], EpubError::MISSING_ROOTFILE);
    expect_error(argv[7], EpubError::MISSING_MANIFEST_ITEM);
    expect_error(argv[8], EpubError::MISSING_SPINE);
    expect_error(argv[9], EpubError::ENCRYPTED);
    expect_error(argv[10], EpubError::UNSUPPORTED_COMPRESSION);
    expect_error(argv[11], EpubError::UNSAFE_PATH);
    expect_error(argv[12], EpubError::TOO_LARGE);
    expect_repeated_text(argv[13], 'x', 65530);
    expect_text(argv[14], "Readable chapter.\n");
    expect_text(argv[15], "Namespaced chapter.\n");
    expect_error(argv[16], EpubError::MISSING_SPINE);
    expect_error(argv[17], EpubError::MALFORMED_ZIP);
    expect_error(argv[18], EpubError::MALFORMED_ZIP);
    expect_text(argv[19], "Stored chapter.\n");
    expect_error(argv[20], EpubError::MALFORMED_ZIP);
    expect_error(argv[21], EpubError::MALFORMED_ZIP);
    expect_error(argv[22], EpubError::MALFORMED_ZIP);
    expect_text(argv[23], "Still visible.\n");
    expect_text(argv[24], "\xE2\x80\x94\xE2\x80\x93\xE2\x80\xA6\xC2\xA9\xE2\x80\x98\xE2\x80\x99\xE2\x80\x9C\xE2\x80\x9D\xC2\xAE\xE2\x84\xA2\n");
    expect_error(argv[25], EpubError::ZIP64);
    expect_error(argv[26], EpubError::ZIP64);
    expect_error(argv[27], EpubError::ZIP64);
    expect_error(argv[28], EpubError::MALFORMED_ZIP);
    expect_error(argv[29], EpubError::MALFORMED_ZIP);
    expect_text(argv[30], "Declared first.\nRight manifest.\n");
    expect_text(argv[31], "Visible\nsingle\n");
    expect_error(argv[32], EpubError::MISSING_MANIFEST_ITEM);
    expect_error(argv[33], EpubError::INVALID_XHTML);
    expect_text(argv[34], "Stored chapter.\n");
    expect_error(argv[35], EpubError::MALFORMED_ZIP);
    expect_error(argv[36], EpubError::MALFORMED_ZIP);
    expect_error(argv[37], EpubError::MALFORMED_ZIP);
    expect_text(argv[38], "Stored chapter.\n");
    expect_text(argv[39], "Stored chapter.\n");
    expect_error(argv[40], EpubError::MALFORMED_ZIP);
    expect_error(argv[41], EpubError::MALFORMED_ZIP);
    expect_error(argv[42], EpubError::MALFORMED_ZIP);
    expect_error(argv[43], EpubError::MALFORMED_ZIP);
    expect_text(argv[44], "Many assets, readable text.\n");
    expect_text(argv[45], "Image skipped, text readable.\n");

    expect_repeated_text(argv[46], 'w', 40000);
    FileSource window_archive(argv[46]); EpubDocument window_book; assert(window_book.open(window_archive));
    assert(window_book.byte_at(33000, value) && value == 'w');
    assert(window_book.byte_at(5, value) && value == 'w');
    assert(window_book.byte_at(16383, value) && value == 'w');
    assert(window_book.byte_at(16384, value) && value == 'w');
    assert(window_book.byte_at(39999, value) && value == 'w');
    assert(window_book.byte_at(40000, value) && value == '\n');

    FileSource boundary_archive(argv[47]); EpubDocument boundary_book; assert(boundary_book.open(boundary_archive));
    assert(boundary_book.size() == 16375);
    assert(boundary_book.byte_at(500, value) && value == 'a');
    assert(boundary_book.byte_at(501, value) && value == '&');
    assert(boundary_book.byte_at(502, value) && value == 'b');
    assert(boundary_book.byte_at(16368, value) && value == 'b');
    assert(boundary_book.byte_at(16369, value) && value == '\n');
    assert(boundary_book.byte_at(16370, value) && value == 't');
    assert(boundary_book.byte_at(16374, value) && value == '\n');
    expect_error(argv[48], EpubError::MALFORMED_ZIP);

    FailingSource failed(ordered, ordered.size() - 10); EpubDocument failed_book;
    assert(! failed_book.open(failed)); assert(failed_book.error() == EpubError::READ_FAILED);
    std::puts("PASS: EPUB package, text, safety, navigation");
}
