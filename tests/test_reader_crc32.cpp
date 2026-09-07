#include "reader_crc32.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace reader;

// Independent, intentionally slow reference; never used by production.
static uint32_t reference(const unsigned char* bytes, size_t size)
{
    uint32_t c = 0xffffffffu;
    for(size_t i = 0; i < size; ++i) {
        c ^= bytes[i];
        for(int bit = 0; bit < 8; ++bit) c = (c & 1) ? (c >> 1) ^ 0xedb88320u : c >> 1;
    }
    return ~c;
}
int main()
{
    const auto* check = reinterpret_cast<const unsigned char*>("123456789");
    assert(crc32_bytes(nullptr, 0) == 0);
    assert(crc32_update(0x12345678u, nullptr, 0) == 0x12345678u);
    assert(crc32_bytes(check, 9) == 0xcbf43926u);
    assert(crc32_combine(0x9be3e0a3u, 0x131da070u, 5) == 0xcbf43926u); // "1234" + "56789"
    std::vector<unsigned char> bytes(65539);
    uint32_t state = 0x923abcu;
    for(auto& byte : bytes) { state = state * 1664525u + 1013904223u; byte = state >> 24; }
    for(size_t n : {size_t(0), size_t(1), size_t(16), size_t(255), size_t(512), size_t(8192), bytes.size()}) {
        const auto expected = reference(bytes.data(), n);
        assert(crc32_bytes(bytes.data(), n) == expected);
        for(size_t split : {size_t(0), n / 3, n / 2, n}) {
            const auto first = reference(bytes.data(), split);
            const auto second = reference(bytes.data() + split, n - split);
            assert(crc32_combine(first, second, n - split) == expected);
            assert(~crc32_update(~first, bytes.data() + split, n - split) == expected);
        }
    }
    uint32_t combined = 0;
    for(size_t at = 0; at + 4096 <= bytes.size(); at += 4096) {
        const auto block = reference(bytes.data() + at, 4096);
        assert(crc32_combine_4096(combined, block) == crc32_combine(combined, block, 4096));
        combined = crc32_combine_4096(combined, block);
        assert(combined == reference(bytes.data(), at + 4096));
    }
    // All byte values and unaligned buffers, including both nibbles of each byte.
    for(unsigned i = 0; i < 256; ++i) bytes[i + 1] = i;
    assert(crc32_bytes(bytes.data() + 1, 256) == 0x29058c73u);
    std::puts("PASS: shared CRC reference vectors, incremental updates and combinations");
}
