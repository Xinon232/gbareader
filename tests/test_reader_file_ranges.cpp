#include "reader_file.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

using namespace reader;
namespace {
std::vector<unsigned char> raw(20003);
unsigned reads = 0;
bool bad_seek = false, bad_read = false, short_read = false;
}
extern "C" {
FRESULT f_open(FIL* f, const TCHAR*, BYTE) { *f = {}; f->obj.objsize = raw.size(); return FR_OK; }
FRESULT f_close(FIL*) { return FR_OK; }
FRESULT f_lseek(FIL* f, FSIZE_t at) { if(bad_seek) return FR_DISK_ERR; f->fptr = at; return FR_OK; }
FRESULT f_read(FIL* f, void* out, UINT n, UINT* got) {
    ++reads; *got = 0;
    if(bad_read) return FR_DISK_ERR;
    n = std::min(n, UINT(raw.size() - f->fptr));
    if(short_read) n = std::min(n, UINT(17));
    std::memcpy(out, raw.data() + f->fptr, n); f->fptr += n; *got = n; return FR_OK;
}
}
int main()
{
    // An inherited byte-dispatch fallback is not a ReaderFile bulk implementation.
    using Range = bool (ReaderFile::*)(uint32_t, unsigned char*, uint32_t) const;
    assert((std::is_same<decltype(&ReaderFile::read_range), Range>::value));
    for(size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<unsigned char>(i * 37);
    ReaderFile file;
    unsigned char value = 0, out[10000];
    assert(!file.read_range(0, out, 1));
    assert(file.open_read_only("book.txt"));
    reads = 0;
    const ByteSource& source = file;
    assert(source.read_range(8100, out, sizeof(out)));
    assert(!std::memcmp(out, raw.data() + 8100, sizeof(out)));
    assert(reads == 3); // One refill per window, including the two boundaries.
    assert(file.byte_at(18099, value) && value == raw[18099]);
    assert(reads == 3);
    assert(source.read_range(16384, out, 100));
    assert(reads == 3 && !std::memcmp(out, raw.data() + 16384, 100));
    assert(source.read_range(1, out, 3));
    assert(file.byte_at(2, value) && value == raw[2]);
    assert(source.read_range(file.size() - 3, out, 3));
    assert(!std::memcmp(out, raw.data() + raw.size() - 3, 3));
    assert(source.read_range(file.size(), out, 0));
    assert(!source.read_range(file.size(), out, 1));
    assert(!source.read_range(0xffffffffu, out, 2));
    assert(!source.read_range(0, nullptr, 1));
    for(int fault = 0; fault < 3; ++fault) {
        file.close(); assert(file.open_read_only("book.txt"));
        bad_seek = fault == 0; bad_read = fault == 1; short_read = fault == 2;
        assert(!source.read_range(8100, out, 200));
        bad_seek = bad_read = short_read = false;
        assert(source.read_range(8100, out, 200));
        assert(!std::memcmp(out, raw.data() + 8100, 200));
        assert(file.byte_at(8192, value) && value == raw[8192]);
    }
    file.close(); assert(!source.read_range(0, out, 1));
    std::puts("PASS: ReaderFile block windows, bounds, short reads and errors");
}
